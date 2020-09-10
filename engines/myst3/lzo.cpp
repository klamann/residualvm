/* ResidualVM - A 3D game interpreter
 *
 * ResidualVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the AUTHORS
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "myst3/lzo.h"

#include "common/algorithm.h"
#include "common/endian.h"
#include "common/ptr.h"
#include "common/util.h"

// This is a port of the lzokay library by Jack Andersen and contributors.

// The MIT License
//
// Copyright (c) 2018 Jack Andersen
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

/*
 * Based on documentation from the Linux sources: Documentation/lzo.txt
 * https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Documentation/lzo.txt
 */

namespace Myst3 {

static const size_t Max255Count = size_t(~0) / 255 - 2;

#define NEEDS_IN(count)                 \
	if (inp + (count) > inp_end) {      \
		dstSize = outp - dst;           \
		return LzoResult::InputOverrun; \
	}

#define NEEDS_OUT(count)                 \
	if (outp + (count) > outp_end) {     \
		dstSize = outp - dst;            \
		return LzoResult::OutputOverrun; \
	}

#define CONSUME_ZERO_BYTE_LENGTH     \
	size_t offset;                   \
	{                                \
		const uint8 *old_inp = inp;  \
		while (*inp == 0)            \
			++inp;                   \
		offset = inp - old_inp;      \
		if (offset > Max255Count) {  \
			dstSize = outp - dst;    \
			return LzoResult::Error; \
		}                            \
	}

#define WRITE_ZERO_BYTE_LENGTH(length)        \
	{                                         \
		size_t l;                             \
		for (l = length; l > 255; l -= 255) { \
			*outp++ = 0;                      \
		}                                     \
		*outp++ = l;                          \
	}

static const uint32 M1MaxOffset = 0x0400;
static const uint32 M2MaxOffset = 0x0800;
static const uint32 M3MaxOffset = 0x4000;
static const uint32 M4MaxOffset = 0xbfff;

static const uint32 M1MinLen = 2;
static const uint32 M1MaxLen = 2;
static const uint32 M2MinLen = 3;
static const uint32 M2MaxLen = 8;
static const uint32 M3MinLen = 3;
static const uint32 M3MaxLen = 33;
static const uint32 M4MinLen = 3;
static const uint32 M4MaxLen = 9;

static const uint32 M1Marker = 0x0;
static const uint32 M2Marker = 0x40;
static const uint32 M3Marker = 0x20;
static const uint32 M4Marker = 0x10;

static const uint32 MaxMatchByLengthLen = 34; /* Max M3 len + 1 */
static const uint16 UInt16Max = 65535u;

LzoResult lzoDecompress(const uint8 *src, size_t srcSize,
                        uint8 *dst, size_t initDstSize,
                        size_t &dstSize) {
	dstSize = initDstSize;

	if (srcSize < 3) {
		dstSize = 0;
		return LzoResult::InputOverrun;
	}

	const uint8 *inp = src;
	const uint8 *inp_end = src + srcSize;
	uint8 *outp = dst;
	uint8 *outp_end = dst + dstSize;
	uint8 *lbcur;
	size_t lblen;
	size_t state = 0;
	size_t nstate = 0;

	/* First byte encoding */
	if (*inp >= 22) {
		/* 22..255 : copy literal string
		 *           length = (byte - 17) = 4..238
		 *           state = 4 [ don't copy extra literals ]
		 *           skip byte
		 */
		size_t len = *inp++ - uint8(17);
		NEEDS_IN(len)
		NEEDS_OUT(len)
		for (size_t i = 0; i < len; ++i)
			*outp++ = *inp++;
		state = 4;
	} else if (*inp >= 18) {
		/* 18..21 : copy 0..3 literals
		 *          state = (byte - 17) = 0..3  [ copy <state> literals ]
		 *          skip byte
		 */
		nstate = *inp++ - uint8(17);
		state = nstate;
		NEEDS_IN(nstate)
		NEEDS_OUT(nstate)
		for (size_t i = 0; i < nstate; ++i)
			*outp++ = *inp++;
	}
	/* 0..17 : follow regular instruction encoding, see below. It is worth
	 *         noting that codes 16 and 17 will represent a block copy from
	 *         the dictionary which is empty, and that they will always be
	 *         invalid at this place.
	 */

	while (true) {
		NEEDS_IN(1)
		uint8 inst = *inp++;
		if (inst & 0xC0) {
			/* [M2]
			 * 1 L L D D D S S  (128..255)
			 *   Copy 5-8 bytes from block within 2kB distance
			 *   state = S (copy S literals after this block)
			 *   length = 5 + L
			 * Always followed by exactly one byte : H H H H H H H H
			 *   distance = (H << 3) + D + 1
			 *
			 * 0 1 L D D D S S  (64..127)
			 *   Copy 3-4 bytes from block within 2kB distance
			 *   state = S (copy S literals after this block)
			 *   length = 3 + L
			 * Always followed by exactly one byte : H H H H H H H H
			 *   distance = (H << 3) + D + 1
			 */
			NEEDS_IN(1)
			lbcur = outp - ((*inp++ << 3) + ((inst >> 2) & 0x7) + 1);
			lblen = size_t(inst >> 5) + 1;
			nstate = inst & uint8(0x3);
		} else if (inst & M3Marker) {
			/* [M3]
			 * 0 0 1 L L L L L  (32..63)
			 *   Copy of small block within 16kB distance (preferably less than 34B)
			 *   length = 2 + (L ?: 31 + (zero_bytes * 255) + non_zero_byte)
			 * Always followed by exactly one LE16 :  D D D D D D D D : D D D D D D S S
			 *   distance = D + 1
			 *   state = S (copy S literals after this block)
			 */
			lblen = size_t(inst & uint8(0x1f)) + 2;
			if (lblen == 2) {
				CONSUME_ZERO_BYTE_LENGTH
				NEEDS_IN(1)
				lblen += offset * 255 + 31 + *inp++;
			}
			NEEDS_IN(2)
			nstate = READ_LE_UINT16(inp);
			inp += 2;
			lbcur = outp - ((nstate >> 2) + 1);
			nstate &= 0x3;
		} else if (inst & M4Marker) {
			/* [M4]
			 * 0 0 0 1 H L L L  (16..31)
			 *   Copy of a block within 16..48kB distance (preferably less than 10B)
			 *   length = 2 + (L ?: 7 + (zero_bytes * 255) + non_zero_byte)
			 * Always followed by exactly one LE16 :  D D D D D D D D : D D D D D D S S
			 *   distance = 16384 + (H << 14) + D
			 *   state = S (copy S literals after this block)
			 *   End of stream is reached if distance == 16384
			 */
			lblen = size_t(inst & uint8(0x7)) + 2;
			if (lblen == 2) {
				CONSUME_ZERO_BYTE_LENGTH
				NEEDS_IN(1)
				lblen += offset * 255 + 7 + *inp++;
			}
			NEEDS_IN(2)
			nstate = READ_LE_UINT16(inp);
			inp += 2;
			lbcur = outp - (((inst & 0x8) << 11) + (nstate >> 2));
			nstate &= 0x3;
			if (lbcur == outp)
				break; /* Stream finished */
			lbcur -= 16384;
		} else {
			/* [M1] Depends on the number of literals copied by the last instruction. */
			if (state == 0) {
				/* If last instruction did not copy any literal (state == 0), this
				 * encoding will be a copy of 4 or more literal, and must be interpreted
				 * like this :
				 *
				 *    0 0 0 0 L L L L  (0..15)  : copy long literal string
				 *    length = 3 + (L ?: 15 + (zero_bytes * 255) + non_zero_byte)
				 *    state = 4  (no extra literals are copied)
				 */
				size_t len = inst + 3;
				if (len == 3) {
					CONSUME_ZERO_BYTE_LENGTH
					NEEDS_IN(1)
					len += offset * 255 + 15 + *inp++;
				}
				/* copy_literal_run */
				NEEDS_IN(len)
				NEEDS_OUT(len)
				for (size_t i = 0; i < len; ++i)
					*outp++ = *inp++;
				state = 4;
				continue;
			} else if (state != 4) {
				/* If last instruction used to copy between 1 to 3 literals (encoded in
				 * the instruction's opcode or distance), the instruction is a copy of a
				 * 2-byte block from the dictionary within a 1kB distance. It is worth
				 * noting that this instruction provides little savings since it uses 2
				 * bytes to encode a copy of 2 other bytes but it encodes the number of
				 * following literals for free. It must be interpreted like this :
				 *
				 *    0 0 0 0 D D S S  (0..15)  : copy 2 bytes from <= 1kB distance
				 *    length = 2
				 *    state = S (copy S literals after this block)
				 *  Always followed by exactly one byte : H H H H H H H H
				 *    distance = (H << 2) + D + 1
				 */
				NEEDS_IN(1)
				nstate = inst & uint8(0x3);
				lbcur = outp - ((inst >> 2) + (*inp++ << 2) + 1);
				lblen = 2;
			} else {
				/* If last instruction used to copy 4 or more literals (as detected by
				 * state == 4), the instruction becomes a copy of a 3-byte block from the
				 * dictionary from a 2..3kB distance, and must be interpreted like this :
				 *
				 *    0 0 0 0 D D S S  (0..15)  : copy 3 bytes from 2..3 kB distance
				 *    length = 3
				 *    state = S (copy S literals after this block)
				 *  Always followed by exactly one byte : H H H H H H H H
				 *    distance = (H << 2) + D + 2049
				 */
				NEEDS_IN(1)
				nstate = inst & uint8(0x3);
				lbcur = outp - ((inst >> 2) + (*inp++ << 2) + 2049);
				lblen = 3;
			}
		}
		if (lbcur < dst) {
			dstSize = outp - dst;
			return LzoResult::LookbehindOverrun;
		}
		NEEDS_IN(nstate)
		NEEDS_OUT(lblen + nstate)
		/* Copy lookbehind */
		for (size_t i = 0; i < lblen; ++i)
			*outp++ = *lbcur++;
		state = nstate;
		/* Copy literal */
		for (size_t i = 0; i < nstate; ++i)
			*outp++ = *inp++;
	}

	dstSize = outp - dst;
	if (lblen != 3) /* Ensure terminating M4 was encountered */
		return LzoResult::Error;
	if (inp == inp_end)
		return LzoResult::Success;
	else if (inp < inp_end)
		return LzoResult::InputNotConsumed;
	else
		return LzoResult::InputOverrun;
}

static const uint32 DictHashSize = 0x4000;
static const uint32 DictMaxDist = 0xbfff;
static const uint32 DictMaxMatchLen = 0x800;
static const uint32 DictBufSize = DictMaxDist + DictMaxMatchLen;

struct State {
	const uint8 *src;
	const uint8 *src_end;
	const uint8 *inp;
	uint32 wind_sz;
	uint32 wind_b;
	uint32 wind_e;
	uint32 cycle1_countdown;

	const uint8 *bufp;
	uint32 buf_sz;

	/* Access next input byte and advance both ends of circular buffer */
	void getByte(uint8 *buf) {
		if (inp >= src_end) {
			if (wind_sz > 0)
				--wind_sz;
			buf[wind_e] = 0;
			if (wind_e < DictMaxMatchLen)
				buf[DictBufSize + wind_e] = 0;
		} else {
			buf[wind_e] = *inp;
			if (wind_e < DictMaxMatchLen)
				buf[DictBufSize + wind_e] = *inp;
			++inp;
		}
		if (++wind_e == DictBufSize)
			wind_e = 0;
		if (++wind_b == DictBufSize)
			wind_b = 0;
	}

	uint32 pos2off(uint32 pos) const {
		return wind_b > pos ? wind_b - pos : DictBufSize - (pos - wind_b);
	}
};

class Dict {
public:
	/* List encoding of previous 3-byte data matches */
	struct Match3 {
		uint16 head[DictHashSize];     /* key -> chain-head-pos */
		uint16 chain_sz[DictHashSize]; /* key -> chain-size */
		uint16 chain[DictBufSize];     /* chain-pos -> next-chain-pos */
		uint16 best_len[DictBufSize];  /* chain-pos -> best-match-length */

		static uint32 makeKey(const uint8 *data) {
			return ((0x9f5f * (((uint32(data[0]) << 5 ^ uint32(data[1])) << 5) ^ data[2])) >> 5) & 0x3fff;
		}

		uint16 getHead(uint32 key) const {
			return (chain_sz[key] == 0) ? UInt16Max : head[key];
		}

		void init() {
			memset(chain_sz, 0, sizeof(chain_sz));
		}

		void remove(uint32 pos, const uint8 *b) {
			--chain_sz[makeKey(b + pos)];
		}

		void advance(State &s, uint32 &match_pos, uint32 &match_count, const uint8 *b) {
			uint32 key = makeKey(b + s.wind_b);
			match_pos = chain[s.wind_b] = getHead(key);
			match_count = chain_sz[key]++;
			if (match_count > DictMaxMatchLen)
				match_count = DictMaxMatchLen;
			head[key] = uint16(s.wind_b);
		}

		void skipAdvance(State &s, const uint8 *b) {
			uint32 key = makeKey(b + s.wind_b);
			chain[s.wind_b] = getHead(key);
			head[key] = uint16(s.wind_b);
			best_len[s.wind_b] = uint16(DictMaxMatchLen + 1);
			chain_sz[key]++;
		}
	};

	/* Encoding of 2-byte data matches */
	struct Match2 {
		uint16 head[1 << 16]; /* 2-byte-data -> head-pos */

		static uint32 makeKey(const uint8 *data) {
			return uint32(data[0]) ^ (uint32(data[1]) << 8);
		}

		void init() {
			memset(head, 0xFF, sizeof(head));
		}

		void add(uint16 pos, const uint8 *b) {
			head[makeKey(b + pos)] = pos;
		}

		void remove(uint32 pos, const uint8 *b) {
			uint16 &p = head[makeKey(b + pos)];
			if (p == pos)
				p = UInt16Max;
		}

		bool search(State &s, uint32 &lb_pos, uint32 &lb_len,
		            uint32 best_pos[MaxMatchByLengthLen], const uint8 *b) const {
			uint16 pos = head[makeKey(b + s.wind_b)];
			if (pos == UInt16Max)
				return false;
			if (best_pos[2] == 0)
				best_pos[2] = pos + 1;
			if (lb_len < 2) {
				lb_len = 2;
				lb_pos = pos;
			}
			return true;
		}
	};

	void init(State &s, const uint8 *src, size_t src_size) {
		s.cycle1_countdown = DictMaxDist;
		_match3.init();
		_match2.init();

		s.src = src;
		s.src_end = src + src_size;
		s.inp = src;
		s.wind_sz = uint32(MIN(src_size, size_t(DictMaxMatchLen)));
		s.wind_b = 0;
		s.wind_e = s.wind_sz;
		memcpy(_buffer, s.inp, s.wind_sz);
		s.inp += s.wind_sz;

		if (s.wind_e == DictBufSize)
			s.wind_e = 0;

		if (s.wind_sz < 3)
			memset(_buffer + s.wind_b + s.wind_sz, 0, 3);
	}

	void resetNextInputEntry(State &s, Match3 &match3, Match2 &match2) {
		/* Remove match from about-to-be-clobbered buffer entry */
		if (s.cycle1_countdown == 0) {
			match3.remove(s.wind_e, _buffer);
			match2.remove(s.wind_e, _buffer);
		} else {
			--s.cycle1_countdown;
		}
	}

	uint32 mismatch(uint8 *first1, uint8 *last1, uint8 *first2) {
		uint8 *pos1 = first1;
		uint8 *pos2 = first2;
		while (pos1 != last1 && *pos1 == *pos2) {
			++pos1;
			++pos2;
		}
		return pos1 - first1;
	}

	void advance(State &s, uint32 &lb_off, uint32 &lb_len,
	             uint32 best_off[MaxMatchByLengthLen], bool skip) {
		if (skip) {
			for (uint32 i = 0; i < lb_len - 1; ++i) {
				resetNextInputEntry(s, _match3, _match2);
				_match3.skipAdvance(s, _buffer);
				_match2.add(uint16(s.wind_b), _buffer);
				s.getByte(_buffer);
			}
		}

		lb_len = 1;
		lb_off = 0;
		uint32 lb_pos = 0;

		uint32 best_pos[MaxMatchByLengthLen];
		Common::fill(best_pos, best_pos + MaxMatchByLengthLen, 0);

		uint32 match_pos, match_count;
		_match3.advance(s, match_pos, match_count, _buffer);

		int best_char = _buffer[s.wind_b];
		uint32 best_len = lb_len;
		if (lb_len >= s.wind_sz) {
			if (s.wind_sz == 0)
				best_char = -1;
			lb_off = 0;
			_match3.best_len[s.wind_b] = DictMaxMatchLen + 1;
		} else {
			if (_match2.search(s, lb_pos, lb_len, best_pos, _buffer) && s.wind_sz >= 3) {
				for (uint32 i = 0; i < match_count; ++i, match_pos = _match3.chain[match_pos]) {
					uint8 *ref_ptr = _buffer + s.wind_b;
					uint8 *match_ptr = _buffer + match_pos;
					uint32 match_len = mismatch(ref_ptr, ref_ptr + s.wind_sz, match_ptr);
					if (match_len < 2)
						continue;
					if (match_len < MaxMatchByLengthLen && best_pos[match_len] == 0)
						best_pos[match_len] = match_pos + 1;
					if (match_len > lb_len) {
						lb_len = match_len;
						lb_pos = match_pos;
						if (match_len == s.wind_sz || match_len > _match3.best_len[match_pos])
							break;
					}
				}
			}
			if (lb_len > best_len)
				lb_off = s.pos2off(lb_pos);
			_match3.best_len[s.wind_b] = uint16(lb_len);
			for (uint *posit = best_pos + 2, *offit = best_off + 2;
			     posit != (best_pos + MaxMatchByLengthLen); ++posit, ++offit) {
				*offit = (*posit > 0) ? s.pos2off(*posit - 1) : 0;
			}
		}

		resetNextInputEntry(s, _match3, _match2);

		_match2.add(uint16(s.wind_b), _buffer);

		s.getByte(_buffer);

		if (best_char < 0) {
			s.buf_sz = 0;
			lb_len = 0;
			/* Signal exit */
		} else {
			s.buf_sz = s.wind_sz + 1;
		}
		s.bufp = s.inp - s.buf_sz;
	}

	Match3 _match3;
	Match2 _match2;

	/* Circular buffer caching enough data to access the maximum lookback
	 * distance of 48K + maximum match length of 2K. An additional 2K is
	 * allocated so the start of the buffer may be replicated at the end,
	 * therefore providing efficient circular access.
	 */
	uint8 _buffer[DictBufSize + DictMaxMatchLen];

	friend struct State;
	friend LzoResult lzoCompress(const uint8 *src, size_t src_size,
	                        uint8 *dst, size_t &dst_size);
};

static void findBetterMatch(const uint32 best_off[MaxMatchByLengthLen], uint32 &lb_len, uint32 &lb_off) {
	if (lb_len <= M2MinLen || lb_off <= M2MaxOffset)
		return;
	if (lb_off > M2MaxOffset && lb_len >= M2MinLen + 1 && lb_len <= M2MaxLen + 1 &&
	    best_off[lb_len - 1] != 0 && best_off[lb_len - 1] <= M2MaxOffset) {
		lb_len -= 1;
		lb_off = best_off[lb_len];
	} else if (lb_off > M3MaxOffset && lb_len >= M4MaxLen + 1 && lb_len <= M2MaxLen + 2 &&
	           best_off[lb_len - 2] && best_off[lb_len] <= M2MaxOffset) {
		lb_len -= 2;
		lb_off = best_off[lb_len];
	} else if (lb_off > M3MaxOffset && lb_len >= M4MaxLen + 1 && lb_len <= M3MaxLen + 1 &&
	           best_off[lb_len - 1] != 0 && best_off[lb_len - 2] <= M3MaxOffset) {
		lb_len -= 1;
		lb_off = best_off[lb_len];
	}
}

static LzoResult encodeLiteralRun(uint8 *&outp, const uint8 *outp_end, const uint8 *dst, size_t &dstSize,
                                  const uint8 *lit_ptr, uint32 lit_len) {
	if (outp == dst && lit_len <= 238) {
		NEEDS_OUT(1);
		*outp++ = uint8(17 + lit_len);
	} else if (lit_len <= 3) {
		outp[-2] = uint8(outp[-2] | lit_len);
	} else if (lit_len <= 18) {
		NEEDS_OUT(1);
		*outp++ = uint8(lit_len - 3);
	} else {
		NEEDS_OUT((lit_len - 18) / 255 + 2);
		*outp++ = 0;
		WRITE_ZERO_BYTE_LENGTH(lit_len - 18);
	}
	NEEDS_OUT(lit_len);
	memcpy(outp, lit_ptr, lit_len);
	outp += lit_len;
	return LzoResult::Success;
}

static LzoResult encodeLookbackMatch(uint8 *&outp, const uint8 *outp_end, const uint8 *dst, size_t &dstSize,
                                     uint32 lb_len, uint32 lb_off, uint32 last_lit_len) {
	if (lb_len == 2) {
		lb_off -= 1;
		NEEDS_OUT(2);
		*outp++ = uint8(M1Marker | ((lb_off & 0x3) << 2));
		*outp++ = uint8(lb_off >> 2);
	} else if (lb_len <= M2MaxLen && lb_off <= M2MaxOffset) {
		lb_off -= 1;
		NEEDS_OUT(2);
		*outp++ = uint8((lb_len - 1) << 5 | ((lb_off & 0x7) << 2));
		*outp++ = uint8(lb_off >> 3);
	} else if (lb_len == M2MinLen && lb_off <= M1MaxOffset + M2MaxOffset && last_lit_len >= 4) {
		lb_off -= 1 + M2MaxOffset;
		NEEDS_OUT(2);
		*outp++ = uint8(M1Marker | ((lb_off & 0x3) << 2));
		*outp++ = uint8(lb_off >> 2);
	} else if (lb_off <= M3MaxOffset) {
		lb_off -= 1;
		if (lb_len <= M3MaxLen) {
			NEEDS_OUT(1);
			*outp++ = uint8(M3Marker | (lb_len - 2));
		} else {
			lb_len -= M3MaxLen;
			NEEDS_OUT(lb_len / 255 + 2);
			*outp++ = uint8(M3Marker);
			WRITE_ZERO_BYTE_LENGTH(lb_len);
		}
		NEEDS_OUT(2);
		*outp++ = uint8(lb_off << 2);
		*outp++ = uint8(lb_off >> 6);
	} else {
		lb_off -= 0x4000;
		if (lb_len <= M4MaxLen) {
			NEEDS_OUT(1);
			*outp++ = uint8(M4Marker | ((lb_off & 0x4000) >> 11) | (lb_len - 2));
		} else {
			lb_len -= M4MaxLen;
			NEEDS_OUT(lb_len / 255 + 2);
			*outp++ = uint8(M4Marker | ((lb_off & 0x4000) >> 11));
			WRITE_ZERO_BYTE_LENGTH(lb_len);
		}
		NEEDS_OUT(2);
		*outp++ = uint8(lb_off << 2);
		*outp++ = uint8(lb_off >> 6);
	}
	return LzoResult::Success;
}

LzoResult lzoCompress(const uint8 *src, size_t srcSize,
                      uint8 *dst, size_t initDstSize,
                      size_t &dstSize) {
	Common::ScopedPtr<Dict> d(new Dict());
	LzoResult err;
	State s;
	dstSize = initDstSize;
	uint8 *outp = dst;
	uint8 *outp_end = dst + dstSize;
	uint32 lit_len = 0;
	uint32 lb_off, lb_len;
	uint32 best_off[MaxMatchByLengthLen];
	d->init(s, src, srcSize);
	const uint8 *lit_ptr = s.inp;
	d->advance(s, lb_off, lb_len, best_off, false);
	while (s.buf_sz > 0) {
		if (lit_len == 0)
			lit_ptr = s.bufp;
		if (lb_len < 2 || (lb_len == 2 && (lb_off > M1MaxOffset || lit_len == 0 || lit_len >= 4)) ||
		    (lb_len == 2 && outp == dst) || (outp == dst && lit_len == 0)) {
			lb_len = 0;
		} else if (lb_len == M2MinLen && lb_off > M1MaxOffset + M2MaxOffset && lit_len >= 4) {
			lb_len = 0;
		}
		if (lb_len == 0) {
			++lit_len;
			d->advance(s, lb_off, lb_len, best_off, false);
			continue;
		}
		findBetterMatch(best_off, lb_len, lb_off);
		if ((err = encodeLiteralRun(outp, outp_end, dst, dstSize, lit_ptr, lit_len)) < LzoResult::Success)
			return err;
		if ((err = encodeLookbackMatch(outp, outp_end, dst, dstSize, lb_len, lb_off, lit_len)) < LzoResult::Success)
			return err;
		lit_len = 0;
		d->advance(s, lb_off, lb_len, best_off, true);
	}
	if ((err = encodeLiteralRun(outp, outp_end, dst, dstSize, lit_ptr, lit_len)) < LzoResult::Success)
		return err;

	/* Terminating M4 */
	NEEDS_OUT(3);
	*outp++ = M4Marker | 1;
	*outp++ = 0;
	*outp++ = 0;

	dstSize = outp - dst;
	return LzoResult::Success;
}

} // namespace lzokay
