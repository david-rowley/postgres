/*-------------------------------------------------------------------------
 *
 * kwlookup.c
 *	  Key word lookup for PostgreSQL
 *
 *
 * Portions Copyright (c) 1996-2025, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/common/kwlookup.c
 *
 *-------------------------------------------------------------------------
 */
#include "c.h"

#include "common/kwlookup.h"

/*
 * ascii_tolower_32
 *		'octets' is an uint32 represending 4 chars.  The return value is a
 *		uint32 version of the input value but with the ASCII character values
 *		folded to lower case.
 */
static inline uint32
ascii_tolower_32(uint32 octets)
{
	uint32 all_bytes = 0x01010101;

	uint32 heptets = octets & (0x7F * all_bytes);

	uint32 is_gt_Z = heptets + (0x7F - 'Z') * all_bytes;

	uint32 is_ge_A = heptets + (0x80 - 'A') * all_bytes;

	uint32 is_ascii = ~octets;

	uint32 is_upper = is_ascii & (is_ge_A ^ is_gt_Z);

	uint32 to_lower = (is_upper >> 2) & (0x20 * all_bytes);

	return (octets | to_lower);
}

/*
 * matches_ascii_lowercase_string
 *		Return true if 'mixed_case' matches 'lower_case' using an ASCII case
 *		insensitive comparison for ASCII characters.  'lower_case' is expected
 *		not to contain any upper-case ASCII characters.
 *
 * Both input strings must have a strlen() equal to 'len'.
 * Neither of the input strings are modified by this function.
 */
static inline bool
matches_ascii_lowercase_string(const char *mixed_case, const char *lower_case,
							   size_t len)
{
	const char *mc = mixed_case;
	const char *lc = lower_case;

	/* ensure 'len' is set correctly */
	Assert(strlen(mixed_case) == len);
	Assert(strlen(lower_case) == len);

	/*
	 * Vectorize comparison by lower casing 4 bytes at a time before
	 * comparing.  We could easily do 8 bytes at a time here, but keywords
	 * are short and 4 bytes at a time, on average, reduces the number of
	 * remaining bytes to process at the end.
	 */
	while (len >= sizeof(uint32))
	{
		uint32 lowered;
		uint32 lcbits;

		memcpy(&lowered, mc, sizeof(uint32));
		memcpy(&lcbits, lc, sizeof(uint32));

		/* fold to lower case */
		lowered = ascii_tolower_32(lowered);

		if (lowered != lcbits)
			return false;

		mc += sizeof(uint32);
		lc += sizeof(uint32);
		len -= sizeof(uint32);
	}

	/* handle remainder */
	while (len-- > 0)
	{
		char ch = *mc++;

		if (ch >= 'A' && ch <= 'Z')
			ch += 'a' - 'A';
		if (ch != *lc++)
			return false;
	}

	return true;
}

/*
 * ScanKeywordLookup - see if a given word is a keyword
 *
 * The list of keywords to be matched against is passed as a ScanKeywordList.
 *
 * Returns the keyword number (0..N-1) of the keyword, or -1 if no match.
 * Callers typically use the keyword number to index into information
 * arrays, but that is no concern of this code.
 *
 * The match is done case-insensitively.  Note that we deliberately use a
 * dumbed-down case conversion that will only translate 'A'-'Z' into 'a'-'z',
 * even if we are in a locale where tolower() would produce more or different
 * translations.  This is to conform to the SQL99 spec, which says that
 * keywords are to be matched in this way even though non-keyword identifiers
 * receive a different case-normalization mapping.
 */
int
ScanKeywordLookup(const char *str, size_t len,
				  const ScanKeywordList *keywords)
{
	int			h;
	const char *kw;

	/*
	 * Reject immediately if too long to be any keyword.  This saves useless
	 * hashing and downcasing work on long strings.
	 */
	if (len > keywords->max_kw_len)
		return -1;

	/*
	 * Compute the hash function.  We assume it was generated to produce
	 * case-insensitive results.  Since it's a perfect hash, we need only
	 * match to the specific keyword it identifies.
	 */
	h = keywords->hash(str, len);

	/* An out-of-range result implies no match */
	if (h < 0 || h >= keywords->num_keywords)
		return -1;

	/* no match unless the lengths are the same */
	if (GetScanKeywordLength(h, keywords) != len)
		return -1;

	kw = GetScanKeyword(h, keywords);

	/*
	 * Compare the input with kw to see if we have a match.  We apply ASCII
	 * only downcasing to the input string.  We must not use tolower() since
	 * it may produce the wrong translation in some locales (eg, Turkish).
	 */
	if (matches_ascii_lowercase_string(str, kw, len))
		return h;

	/* no match */
	return -1;
}
