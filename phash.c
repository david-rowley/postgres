#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef unsigned long long uint64;
typedef long long int64;
typedef unsigned int uint32;
typedef int int32;
typedef unsigned short uint16;
typedef short int16;
typedef unsigned char uint8;
typedef char int8;

typedef struct Keyword {
	char *keyword;
	int32 kw_index;
} Keyword;

Keyword *keywords = NULL;
uint32 nkeywords = 0;
uint32 keywords_size = 0;

typedef struct KeywordLengthSpecific {
	uint32	keywordlen;		/* keyword length */
	uint32	hashway;	/* how many times is the hashkeysize split (how many indexes of startpositions are valid) */
	uint32	hashkeysize;	/* 2, 4 or 8 bytes */
	uint32	hashseed;			/* multiplier for hashing */
	uint32	rightshift;			/* how many bits to shift right by */
	uint32	startpositions[4];	/* start positions into word for hashing */
	uint32	nbuckets;		/* What to mod the hash value by */
} KeywordLengthSpecific;

KeywordLengthSpecific *keywordlengthspecific = NULL;
uint32 nkeywordlengthspecifics = 0;
uint32 keywordlengthspecific_size = 0;

int16 *lookupbuckets = NULL;
uint32 nlookupbuckets = 0;
uint32 lookupbuckets_size = 0;

typedef struct Options {
	char	*prefix;				/* Prefix for the output global variable names */
	bool	NullTerminateKeyWords;	/* Keywords go into a single string const, should we put a NUL terminator between each? */
	bool	EmptyHashSlotReturnFirstKeyWord;	/* Store 0 instead of -1 in empty perfect hash slots */
	bool	ExtraOffsetElement; /* Useful so it's possible to get length of keyword by always being table to subtract offset from next keyword's offset */
	bool	CaseInsensitive;
	bool	Verbose;
} Options;

static uint16
preparekey_2by1(const char *word, uint32 wordlen, uint32 *startpos, Options *options)
{
	uint16 value;
	memcpy(&value, word + startpos[0], 2);

	if (options->CaseInsensitive)
		return value | 0x2020U;
	return value;
}

static uint16
preparekey_2by2(const char *word, uint32 wordlen, uint32 *startpos, Options *options)
{
	uint8 values[2];

	memcpy(&values[0], word + startpos[0], 1);
	memcpy(&values[1], word + startpos[1], 1);
	if (options->CaseInsensitive)
		return ((uint16) (values[1]) << 8 | (uint16) values[0]) | 0x2020U;
	return (uint16) (values[1]) << 8 | (uint16) values[0];
}

static uint32
preparekey_4by1(const char *word, uint32 wordlen, uint32 *startpos, Options *options)
{
	uint32 value;
	memcpy(&value, word + startpos[0], 4);

	if (options->CaseInsensitive)
		return value | 0x20202020U;
	return value;
}

uint32
preparekey_4by2(const char *word, uint32 wordlen, uint32 *startpos, Options *options)
{
	uint16 values[2];

	memcpy(&values[0], word + startpos[0], 2);
	memcpy(&values[1], word + startpos[1], 2);
	if (options->CaseInsensitive)
		return ((uint32) (values[1]) << 16 | (uint32) values[0]) | 0x20202020U;
	return (uint32) (values[1]) << 16 | (uint32) values[0];
}

uint32
preparekey_4by4(const char *word, uint32 wordlen, uint32 *startpos, Options *options)
{
	unsigned char values[4];

	memcpy(&values[0], word + startpos[0], 1);
	memcpy(&values[1], word + startpos[1], 1);
	memcpy(&values[2], word + startpos[2], 1);
	memcpy(&values[3], word + startpos[3], 1);

	if (options->CaseInsensitive)
		return ((uint32) (values[3]) << 24 |
			   (uint32) values[2] << 16 |
			   (uint32) values[1] << 8 |
			   (uint32) values[0]) | 0x20202020U;
	return (uint32) (values[3]) << 24 |
		   (uint32) values[2] << 16 |
		   (uint32) values[1] << 8 |
		   (uint32) values[0];
}

static uint64
preparekey_8by1(const char *word, uint32 wordlen, uint32 *startpos, Options *options)
{
	uint64 value;
	memcpy(&value, word + startpos[0], 8);

	if (options->CaseInsensitive)
		return value | 0x2020202020202020ULL;
	return value;
}

uint64
preparekey_8by2(const char *word, uint32 wordlen, uint32 *startpos, Options *options)
{
	uint32 values[2];

	memcpy(&values[0], word + startpos[0], 4);
	memcpy(&values[1], word + startpos[1], 4);
	if (options->CaseInsensitive)
		return ((uint64) (values[1]) << 32 | (uint64) values[0]) | 0x2020202020202020ULL;
	return (uint64) (values[1]) << 32 | (uint64) values[0];
}

uint64
preparekey_8by4(const char *word, uint32 wordlen, uint32 *startpos, Options *options)
{
	uint16 values[4];

	memcpy(&values[0], word + startpos[0], 2);
	memcpy(&values[1], word + startpos[1], 2);
	memcpy(&values[2], word + startpos[2], 2);
	memcpy(&values[3], word + startpos[3], 2);

	if (options->CaseInsensitive)
		return ((uint64) (values[3]) << 48 |
				(uint64) values[2] << 32 |
				(uint64) values[1] << 16 |
				(uint64) values[0]) | 0x2020202020202020ULL;
	return (uint64) (values[3]) << 48 |
		   (uint64) values[2] << 32 |
		   (uint64) values[1] << 16 |
		   (uint64) values[0];
}

int32
cmp_uint16(const void *pa, const void *pb)
{
	uint16 a = *(uint16 *) pa;
	uint16 b = *(uint16 *) pb;

	return a < b ? -1 : a > b ? +1 : 0;
}

int32
cmp_uint32(const void *pa, const void *pb)
{
	uint32 a = *(uint32 *) pa;
	uint32 b = *(uint32 *) pb;

	return a < b ? -1 : a > b ? +1 : 0;
}

int32
cmp_uint64(const void *pa, const void *pb)
{
	uint64 a = *(uint64 *) pa;
	uint64 b = *(uint64 *) pb;

	return a < b ? -1 : a > b ? +1 : 0;
}

static int
has_duplicates16(uint16 *wordhashes, uint32 numwords)
{
	uint16 *dup = malloc(sizeof(uint16) * numwords);
	uint16 lasthash;

	memcpy(dup, wordhashes, sizeof(uint16) * numwords);

	qsort(dup, numwords, sizeof(uint16), cmp_uint16);

	lasthash = dup[0];
	for (uint32 i = 1; i < numwords; i++)
	{
		if (lasthash == dup[i])
		{
			free(dup);
			return 1;
		}
		lasthash = dup[i];
	}
	free(dup);
	return 0;
}

static int
has_duplicates32(uint32 *wordhashes, uint32 numwords)
{
	uint32 *dup = malloc(sizeof(uint32) * numwords);
	uint32 lasthash;

	memcpy(dup, wordhashes, sizeof(uint32) * numwords);
	qsort(dup, numwords, sizeof(uint32), cmp_uint32);

	lasthash = dup[0];
	for (uint32 i = 1; i < numwords; i++)
	{
		if (lasthash == dup[i])
		{
			free(dup);
			return 1;
		}
		lasthash = dup[i];
	}
	free(dup);
	return 0;
}

static int
has_duplicates64(uint64 *wordhashes, uint32 numwords)
{
	uint64 *dup = malloc(sizeof(uint64) * numwords);
	uint64 lasthash;

	memcpy(dup, wordhashes, sizeof(uint64) * numwords);
	qsort(dup, numwords, sizeof(uint64), cmp_uint64);

	lasthash = dup[0];
	for (uint32 i = 1; i < numwords; i++)
	{
		if (lasthash == dup[i])
		{
			free(dup);
			return 1;
		}
		lasthash = dup[i];
	}
	free(dup);
	return 0;
}

static void addLookupBucket(int16 value);

typedef struct SeedGenerator {
	uint32 total_rounds;
	uint32 rounds_complete;
	bool do_random;
} SeedGenerator;

static void
SeedGenerator_Setup(SeedGenerator *seeder, uint32 rounds)
{
	seeder->total_rounds = rounds;
	seeder->rounds_complete = 0;
	seeder->do_random = false;
}

static uint32 SeedGenerator_NextSeed(SeedGenerator *seeder)
{
	/* These are just seeds I got from searching near-exhaustively for PostgreSQL keywords */
	static const uint32 SeedsToTry[] = {73, 16535903, 66526991, 213128516, 66154, 3689746, 1765843, 4226644, 2047307, 483, 55, 11, 1, 0 };
	uint32 seed;

	if (!seeder->do_random)
	{
		if (seeder->rounds_complete >= sizeof(SeedsToTry) / sizeof(uint32))
		{
			seeder->do_random = true;
			seeder->rounds_complete = 0;
		}
		else
			return SeedsToTry[seeder->rounds_complete++];
	}

	if (seeder->rounds_complete == seeder->total_rounds)
		return 0;

	seeder->rounds_complete++;
	while ((seed = rand()) == 0)
		;
	return seed;
}

#define PH_FUNCNAME searchhash_2by1
#define PH_IDENT "2by1"
#define PH_WAYS 1
#define PH_HASHTYPE uint16
#define PH_KEYSIZE 2
#include "phash.h"

#define PH_FUNCNAME searchhash_2by2
#define PH_IDENT "2by2"
#define PH_WAYS 2
#define PH_HASHTYPE uint16
#define PH_KEYSIZE 2
#include "phash.h"

#define PH_FUNCNAME searchhash_4by1
#define PH_IDENT "4by1"
#define PH_WAYS 1
#define PH_HASHTYPE uint32
#define PH_KEYSIZE 4
#include "phash.h"

#define PH_FUNCNAME searchhash_4by2
#define PH_IDENT "4by2"
#define PH_WAYS 2
#define PH_HASHTYPE uint32
#define PH_KEYSIZE 4
#include "phash.h"

#define PH_FUNCNAME searchhash_4by4
#define PH_IDENT "4by4"
#define PH_WAYS 4
#define PH_HASHTYPE uint32
#define PH_KEYSIZE 4
#include "phash.h"

#define PH_FUNCNAME searchhash_8by1
#define PH_IDENT "8by1"
#define PH_WAYS 1
#define PH_HASHTYPE uint64
#define PH_KEYSIZE 8
#include "phash.h"

#define PH_FUNCNAME searchhash_8by2
#define PH_IDENT "8by2"
#define PH_WAYS 2
#define PH_HASHTYPE uint64
#define PH_KEYSIZE 8
#include "phash.h"

#define PH_FUNCNAME searchhash_8by4
#define PH_IDENT "8by4"
#define PH_WAYS 4
#define PH_HASHTYPE uint64
#define PH_KEYSIZE 8
#include "phash.h"

static int
search(KeywordLengthSpecific *kls, Keyword *words, uint32 numwords, int32 wordlen, uint32 start_buckets, uint32 max_buckets, uint32 rounds, Options *options)
{
	uint32 best_nbuckets;
	uint16 *wordhashes16 = aligned_alloc(64, sizeof(uint16) * numwords);
	uint32 *wordhashes32 = aligned_alloc(64, sizeof(uint32) * numwords);
	uint64 *wordhashes64 = aligned_alloc(64, sizeof(uint64) * numwords);
	int16 *buckets = aligned_alloc(64, sizeof(int16) * max_buckets);

	int found = 0;

	/* 2by1 */
	for (int32 startpos = 0; startpos <= wordlen - 2; startpos++)
	{
		for (uint32 i = 0; i < numwords; i++)
			wordhashes16[i] = preparekey_2by1(words[i].keyword, wordlen, &startpos, options);

		if (has_duplicates16(wordhashes16, numwords))
			continue;

		if (options->Verbose)
			printf("Found 2by1 way unique at pos %d\n", startpos);

		best_nbuckets = searchhash_2by1(kls, words, wordhashes16, numwords, wordlen, &startpos, buckets, start_buckets, max_buckets, rounds, options->Verbose);

		if (best_nbuckets != 0)
		{
			found = 1;
			goto cleanup;
		}
	}

	/* 2by2 */
	for (int32 startpos1 = 0; startpos1 <= wordlen - 1; startpos1++)
	{
		for (int32 startpos2 = 0; startpos2 <= wordlen - 1; startpos2++)
		{
			int32 startpos[2] = { startpos1, startpos2 };

			for (uint32 i = 0; i < numwords; i++)
				wordhashes16[i] = preparekey_2by2(words[i].keyword, wordlen, startpos, options);

			if (has_duplicates16(wordhashes16, numwords))
				continue;

			if (options->Verbose)
				printf("Found 2by2 way unique at pos %d %d\n", startpos[0], startpos[1]);

			best_nbuckets = searchhash_2by2(kls, words, wordhashes16, numwords, wordlen, startpos, buckets, start_buckets, max_buckets, rounds, options->Verbose);

			if (best_nbuckets != 0)
			{
				found = 1;
				goto cleanup;
			}
		}
	}

	if (wordlen >= 2)
	{
		/* 4by1 */
		for (int32 startpos = 0; startpos <= wordlen - 4; startpos++)
		{
			for (uint32 i = 0; i < numwords; i++)
				wordhashes32[i] = preparekey_4by1(words[i].keyword, wordlen, &startpos, options);

			if (has_duplicates32(wordhashes32, numwords))
				continue;

			if (options->Verbose)
				printf("Found 4by1 way unique at pos %d\n", startpos);

			best_nbuckets = searchhash_4by1(kls, words, wordhashes32, numwords, wordlen, &startpos, buckets, start_buckets, max_buckets, rounds, options->Verbose);

			if (best_nbuckets != 0)
			{
				found = 1;
				goto cleanup;
			}
		}

		/* 4by2 */
		for (int32 startpos1 = 0; startpos1 <= wordlen - 2; startpos1++)
		{
			for (int32 startpos2 = 0; startpos2 <= wordlen - 2; startpos2++)
			{
				int32 startpos[2] = { startpos1, startpos2 };

				for (uint32 i = 0; i < numwords; i++)
					wordhashes32[i] = preparekey_4by2(words[i].keyword, wordlen, startpos, options);

				if (has_duplicates32(wordhashes32, numwords))
					continue;

				if (options->Verbose)
					printf("Found 4by2 way unique at pos %d %d\n", startpos[0], startpos[1]);

				best_nbuckets = searchhash_4by2(kls, words, wordhashes32, numwords, wordlen, startpos, buckets, start_buckets, max_buckets, rounds, options->Verbose);

				if (best_nbuckets != 0)
				{
					found = 1;
					goto cleanup;
				}
			}
		}

		/* 4by4 */
		for (int32 startpos1 = 0; startpos1 <= wordlen - 1; startpos1++)
		{
			for (int32 startpos2 = 0; startpos2 <= wordlen - 1; startpos2++)
			{
				for (int32 startpos3 = 0; startpos3 <= wordlen - 1; startpos3++)
				{
					for (int32 startpos4 = 0; startpos4 <= wordlen - 1; startpos4++)
					{
						int32 startpos[4] = { startpos1, startpos2, startpos3, startpos4 };

						for (uint32 i = 0; i < numwords; i++)
							wordhashes32[i] = preparekey_4by4(words[i].keyword, wordlen, startpos, options);

						if (has_duplicates32(wordhashes32, numwords))
							continue;

						if (options->Verbose)
							printf("Found 4by4 way unique at pos %d %d %d %d\n", startpos[0], startpos[1], startpos[2], startpos[3]);

						best_nbuckets = searchhash_4by4(kls, words, wordhashes32, numwords, wordlen, startpos, buckets, start_buckets, max_buckets, rounds, options->Verbose);

						if (best_nbuckets != 0)
						{
							found = 1;
							goto cleanup;
						}
					}
				}
			}
		}
	}

	if (wordlen >= 4)
	{
		/* 8by1 */
		for (int32 startpos = 0; startpos <= wordlen - 8; startpos++)
		{
			for (uint32 i = 0; i < numwords; i++)
				wordhashes64[i] = preparekey_8by1(words[i].keyword, wordlen, &startpos, options);

			if (has_duplicates64(wordhashes64, numwords))
				continue;

			if (options->Verbose)
				printf("Found 8by1 way unique at pos %d\n", startpos);

			best_nbuckets = searchhash_8by1(kls, words, wordhashes64, numwords, wordlen, &startpos, buckets, start_buckets, max_buckets, rounds, options->Verbose);

			if (best_nbuckets != 0)
			{
				found = 1;
				goto cleanup;
			}
		}

		/* 8by2 */
		for (int32 startpos1 = 0; startpos1 <= wordlen - 4; startpos1++)
		{
			for (int32 startpos2 = 0; startpos2 <= wordlen - 4; startpos2++)
			{
				int32 startpos[2] = { startpos1, startpos2 };

				for (uint32 i = 0; i < numwords; i++)
					wordhashes64[i] = preparekey_8by2(words[i].keyword, wordlen, startpos, options);

				if (has_duplicates64(wordhashes64, numwords))
					continue;

				if (options->Verbose)
					printf("Found 8by2 way unique at pos %d %d\n", startpos[0], startpos[1]);

				best_nbuckets = searchhash_8by2(kls, words, wordhashes64, numwords, wordlen, startpos, buckets, start_buckets, max_buckets, rounds, options->Verbose);

				if (best_nbuckets != 0)
				{
					found = 1;
					goto cleanup;
				}
			}
		}

		/* 8by4 */
		for (int32 startpos1 = 0; startpos1 <= wordlen - 2; startpos1++)
		{
			for (int32 startpos2 = 0; startpos2 <= wordlen - 2; startpos2++)
			{
				for (int32 startpos3 = 0; startpos3 <= wordlen - 2; startpos3++)
				{
					for (int32 startpos4 = 0; startpos4 <= wordlen - 2; startpos4++)
					{
						int32 startpos[4] = { startpos1, startpos2, startpos3, startpos4 };

						for (uint32 i = 0; i < numwords; i++)
							wordhashes64[i] = preparekey_8by4(words[i].keyword, wordlen, startpos, options);

						if (has_duplicates64(wordhashes64, numwords))
							continue;

						if (options->Verbose)
							printf("Found 8by4 way unique at pos %d %d %d %d\n", startpos[0], startpos[1], startpos[2], startpos[3]);

						best_nbuckets = searchhash_8by4(kls, words, wordhashes64, numwords, wordlen, startpos, buckets, start_buckets, max_buckets, rounds, options->Verbose);

						if (best_nbuckets != 0)
						{
							found = 1;
							goto cleanup;
						}
					}
				}
			}
		}
	}

cleanup:
	free(buckets);
	free(wordhashes16);
	free(wordhashes32);
	free(wordhashes64);

	return best_nbuckets;
}

int32
guess_initial_hash_size(int32 n)
{
	int32 i = 1;
	int guess = (int) ((double) n * 1);
	while (i < guess)
		i *= 2;
	return i;
}

void
addkeyword(char *keyword, bool verbose)
{
	Keyword *newword;

	if (keywords_size == 0)
	{
		keywords_size = 512;
		keywords = malloc(sizeof(Keyword) * keywords_size);
	}
	else if (nkeywords == keywords_size)
	{
		keywords_size *= 2;
		keywords = realloc(keywords, sizeof(Keyword) * keywords_size);
	}

	newword = &keywords[nkeywords];
	newword->keyword = strdup(keyword);
	newword->kw_index = nkeywords;
	nkeywords++;
}

KeywordLengthSpecific *
addKeywordLengthSpecific(void)
{
	KeywordLengthSpecific *kls;

	if (keywordlengthspecific_size == 0)
	{
		keywordlengthspecific_size = 32;
		keywordlengthspecific = malloc(sizeof(KeywordLengthSpecific) * keywordlengthspecific_size);
	}
	else if (nkeywordlengthspecifics == keywordlengthspecific_size)
	{
		keywordlengthspecific_size *= 2;
		keywordlengthspecific = realloc(keywordlengthspecific, sizeof(KeywordLengthSpecific) * keywordlengthspecific_size);
	}

	kls = &keywordlengthspecific[nkeywordlengthspecifics];

	nkeywordlengthspecifics++;
	return kls;
}

static void
addLookupBucket(int16 value)
{

	if (lookupbuckets_size == 0)
	{
		lookupbuckets_size = 2048;
		lookupbuckets = malloc(sizeof(int16) * lookupbuckets_size);
	}
	else if (nlookupbuckets == lookupbuckets_size)
	{
		lookupbuckets_size *= 2;
		lookupbuckets = realloc(lookupbuckets, sizeof(int16) * lookupbuckets_size);
	}

	lookupbuckets[nlookupbuckets++] = value;
}

int32
cmp_keyword(const void *pa, const void *pb)
{
	Keyword *a = (Keyword *) pa;
	Keyword *b = (Keyword *) pb;
	size_t a_len = strlen(a->keyword);
	size_t b_len = strlen(b->keyword);

	if (a_len == b_len)
		return strcmp(a->keyword, b->keyword);
	return a_len < b_len ? -1 : 1;
}

int32
cmp_keyword_by_name(const void *pa, const void *pb)
{
	Keyword *a = (Keyword *) pa;
	Keyword *b = (Keyword *) pb;

	return strcmp(a->keyword, b->keyword);
}


int
loadkeywords(bool verbose)
{
	char buffer[1024];

	while (fgets(buffer, sizeof(buffer), stdin))
	{
		if (strncmp(buffer,"PG_KEYWORD(\"", sizeof("PG_KEYWORD(\"") - 1) == 0)
		{
			char *keyword = buffer + sizeof("PG_KEYWORD(\"") - 1;
			char *p = keyword;
			char last = '\0';
			bool gotword = false;

			while (*p != '\r' && *p != '\n')
			{
				if (*p == '"' && last != '\\')
				{
					*p = '\0';
					addkeyword(keyword, verbose);
					gotword = true;
				}

				if (gotword)
					break;		/* next line */
				last = *p;
				p++;
			}
		}
	}
	if (verbose)
		printf("Loaded %u keywords\n", nkeywords);
	qsort(keywords, nkeywords, sizeof(Keyword), cmp_keyword);
}

int32
process_word(uint32 wordlen, uint32 rounds, Options *options)
{
	KeywordLengthSpecific *kls;
	uint32 nbuckets;
	uint32 best_nbuckets;
	uint32 i;
	int32  first = -1;
	int32  last = -1;

	for (i = 0; i < nkeywords; i++)
	{
		Keyword *kw = &keywords[i];

		if (strlen(kw->keyword) == wordlen)
		{
			first = i;
			break;
		}
	}

	if (first == -1)
		return -1;

	for (; i < nkeywords; i++)
	{
		Keyword *kw = &keywords[i];

		if (strlen(kw->keyword) != wordlen)
			break;
	}

	last = i;

	if (options->Verbose)
		printf("* Num keywords = %u wordlen = %u\n", last - first, wordlen);

	nbuckets = guess_initial_hash_size(last - first);
	kls = addKeywordLengthSpecific();
	best_nbuckets = search(kls, &keywords[first], last - first, wordlen, nbuckets, 1024, rounds, options);

	if (best_nbuckets != 0)
		return best_nbuckets;
	else
	{
		fprintf(stderr, "Failed to find perfect hash for wordlen = %u\n", wordlen);
		exit(-1);
	}
}

static const char *AsciiLowerCaseMask[] = {
	[1] = "0x20U",
	[2] = "0x2020U",
	[4] = "0x20202020U",
	[8] = "0x2020202020202020U"
};

void
print_final_code(Options *options)
{
	uint32 offsetbase = 0;
	Keyword *alphabetical_keywords = malloc(sizeof(Keyword) * nkeywords);
	uint16  *offsets = malloc(sizeof(uint16) * nkeywords);
	size_t	curoffset;
	memcpy(alphabetical_keywords, keywords, sizeof(Keyword) * nkeywords);
	qsort(alphabetical_keywords, nkeywords, sizeof(Keyword), cmp_keyword_by_name);

	printf("static const char %s_kw_string[] =\n", options->prefix);

	curoffset = 0;
	for (uint32 i = 0; i < nkeywords; i++)
	{
		const char *word = alphabetical_keywords[i].keyword;
		if (i + 1 != nkeywords)
			printf("\t\"%s%s\"\n", word, options->NullTerminateKeyWords ? "\\0" : "");
		else
			printf("\t\"%s\";\n\n", word);

		offsets[i] = (uint16) curoffset;
		curoffset += strlen(word) + (options->NullTerminateKeyWords == true); /* length + \0 */
	}

	if (curoffset >= USHRT_MAX)
	{
		fprintf(stderr, "all key words are longer than 0xffff\n");
		exit(-1);
	}

	/* write out offset array to index the keyword string from the keyword number */
	printf("static const uint16 %s_kw_offsets[] = {\n", options->prefix);
	for (uint32 i = 0; i < nkeywords; i++)
		printf("\t%u,\n", offsets[i]);

	/* Add extra offset element, if requested */
	if (options->ExtraOffsetElement)
		printf("\t%lu,\n", curoffset);
	printf("};\n\n");

	printf("#define %s_NUM_KEYWORDS %u\n\n", options->prefix, nkeywords);

	printf("static const int16 %s_kw_perfecthash[%u] = {\n", options->prefix, nlookupbuckets);

#define ITEMS_PER_LINE 8
	for (uint32 i = 0; i < nlookupbuckets; i++)
	{
		bool notlast = i + 1 != nlookupbuckets;
		int kw_index = lookupbuckets[i];

		/*
		 * Allow an option to store zero instead of -1.  When doing this
		 * we don't write out the -1 check in the lookup function.  This
		 * means it falls back on comparing the given word with the first
		 * keyword.  If most lookups are keywords then this is faster as
		 * it saves a needless -1 check and makes the code smaller.
		 */
		if (options->EmptyHashSlotReturnFirstKeyWord && kw_index == -1)
			kw_index = 0;

		printf("%s%d%s%s", i % ITEMS_PER_LINE == 0 ? "\t" : " ",
			   kw_index,
			   notlast ? "," : "",
			   i % ITEMS_PER_LINE == ITEMS_PER_LINE - 1 && notlast ? "\n" : "");
	}
	printf("\n};\n\n");

	if (options->CaseInsensitive)
	{
		printf("static inline uint64\n");
		printf("ascii_tolower_64(uint64 octets)\n");
		printf("{\n");
		printf("	uint64 all_bytes = 0x0101010101010101;\n");
		printf("\n");
		printf("	uint64 heptets = octets & (0x7F * all_bytes);\n");
		printf("\n");
		printf("	uint64 is_gt_Z = heptets + (0x7F - 'Z') * all_bytes;\n");
		printf("\n");
		printf("	uint64 is_ge_A = heptets + (0x80 - 'A') * all_bytes;\n");
		printf("\n");
		printf("	uint64 is_ascii = ~octets;\n");
		printf("\n");
		printf("	uint64 is_upper = is_ascii & (is_ge_A ^ is_gt_Z);\n");
		printf("\n");
		printf("	uint64 to_lower = (is_upper >> 2) & (0x20 * all_bytes);\n");
		printf("\n");
		printf("	return (octets | to_lower);\n");
		printf("}\n");
		printf("\n");
		printf("static inline uint32\n");
		printf("ascii_tolower_32(uint32 octets)\n");
		printf("{\n");
		printf("	uint32 all_bytes = 0x01010101;\n");
		printf("\n");
		printf("	uint32 heptets = octets & (0x7F * all_bytes);\n");
		printf("\n");
		printf("	uint32 is_gt_Z = heptets + (0x7F - 'Z') * all_bytes;\n");
		printf("\n");
		printf("	uint32 is_ge_A = heptets + (0x80 - 'A') * all_bytes;\n");
		printf("\n");
		printf("	uint32 is_ascii = ~octets;\n");
		printf("\n");
		printf("	uint32 is_upper = is_ascii & (is_ge_A ^ is_gt_Z);\n");
		printf("\n");
		printf("	uint32 to_lower = (is_upper >> 2) & (0x20 * all_bytes);\n");
		printf("\n");
		printf("	return (octets | to_lower);\n");
		printf("}\n");
		printf("\n");
		printf("static inline uint16\n");
		printf("ascii_tolower_16(uint16 octets)\n");
		printf("{\n");
		printf("	uint16 all_bytes = 0x0101;\n");
		printf("\n");
		printf("	uint16 heptets = octets & (0x7F * all_bytes);\n");
		printf("\n");
		printf("	uint16 is_gt_Z = heptets + (0x7F - 'Z') * all_bytes;\n");
		printf("\n");
		printf("	uint16 is_ge_A = heptets + (0x80 - 'A') * all_bytes;\n");
		printf("\n");
		printf("	uint16 is_ascii = ~octets;\n");
		printf("\n");
		printf("	uint16 is_upper = is_ascii & (is_ge_A ^ is_gt_Z);\n");
		printf("\n");
		printf("	uint16 to_lower = (is_upper >> 2) & (0x20 * all_bytes);\n");
		printf("\n");
		printf("	return (octets | to_lower);\n");
		printf("}\n");
		printf("\n");
		printf("static inline bool\n");
		printf("matches_ascii_lowercase_string(const char *mixed_case, const char *lower_case,\n");
		printf("							   size_t len)\n");
		printf("{\n");
		printf("	const char *mc = mixed_case;\n");
		printf("	const char *lc = lower_case;\n");
		printf("\n");
		printf("	/* Vectorize comparison by lower casing doing up to 8 bytes at a time */\n");
		printf("	while (len >= sizeof(uint64))\n");
		printf("	{\n");
		printf("		uint64 lowered;\n");
		printf("		uint64 lcbits;\n");
		printf("\n");
		printf("		memcpy(&lowered, mc, sizeof(uint64));\n");
		printf("		memcpy(&lcbits, lc, sizeof(uint64));\n");
		printf("\n");
		printf("		/* fold to lower case */\n");
		printf("		lowered = ascii_tolower_64(lowered);\n");
		printf("\n");
		printf("		if (lowered != lcbits)\n");
		printf("			return false;\n");
		printf("\n");
		printf("		mc += sizeof(uint64);\n");
		printf("		lc += sizeof(uint64);\n");
		printf("		len -= sizeof(uint64);\n");
		printf("	}\n");
		printf("\n");
		printf("	while (len >= sizeof(uint32))\n");
		printf("	{\n");
		printf("		uint32 lowered;\n");
		printf("		uint32 lcbits;\n");
		printf("\n");
		printf("		memcpy(&lowered, mc, sizeof(uint32));\n");
		printf("		memcpy(&lcbits, lc, sizeof(uint32));\n");
		printf("\n");
		printf("		/* fold to lower case */\n");
		printf("		lowered = ascii_tolower_32(lowered);\n");
		printf("\n");
		printf("		if (lowered != lcbits)\n");
		printf("			return false;\n");
		printf("\n");
		printf("		mc += sizeof(uint32);\n");
		printf("		lc += sizeof(uint32);\n");
		printf("		len -= sizeof(uint32);\n");
		printf("	}\n");
		printf("\n");
		printf("	while (len >= sizeof(uint16))\n");
		printf("	{\n");
		printf("		uint16 lowered;\n");
		printf("		uint16 lcbits;\n");
		printf("\n");
		printf("		memcpy(&lowered, mc, sizeof(uint16));\n");
		printf("		memcpy(&lcbits, lc, sizeof(uint16));\n");
		printf("\n");
		printf("		/* fold to lower case */\n");
		printf("		lowered = ascii_tolower_16(lowered);\n");
		printf("\n");
		printf("		if (lowered != lcbits)\n");
		printf("			return false;\n");
		printf("\n");
		printf("		mc += sizeof(uint16);\n");
		printf("		lc += sizeof(uint16);\n");
		printf("		len -= sizeof(uint16);\n");
		printf("	}\n");
		printf("\n");
		printf("	/* handle remainder */\n");
		printf("	while (len-- > 0)\n");
		printf("	{\n");
		printf("		char ch = *mc++;\n");
		printf("\n");
		printf("		if (ch >= 'A' && ch <= 'Z')\n");
		printf("			ch += 'a' - 'A';\n");
		printf("		if (ch != *lc++)\n");
		printf("			return false;\n");
		printf("	}\n");
		printf("\n");
		printf("	return true;\n");
		printf("}\n");
	}

	for (uint32 i = 0; i < nkeywordlengthspecifics; i++)
	{
		KeywordLengthSpecific *kls = &keywordlengthspecific[i];

		printf("int16 wordlookup%u(const char *word)\n", kls->keywordlen);
		printf("{\n");

		if (kls->nbuckets > 1)
		{
			printf("\tuint32 bucketidx;\n");
			printf("\tuint%u value;\n", kls->hashkeysize * 8);
			printf("\tint16 keywordidx;\n");
			if (kls->hashway == 1)
			{
				printf("\n\tmemcpy(&value, word + %u, %u);\n\n", kls->startpositions[0], kls->hashkeysize);
				if (options->CaseInsensitive)
					printf("\tvalue |= %s;\n", AsciiLowerCaseMask[kls->hashkeysize]);
			}
			else
			{
				printf("\tuint%u values[%u];\n\n", kls->hashkeysize * 8 / kls->hashway, kls->hashway);

				for (uint32 j = 0; j < kls->hashway; j++)
					printf("\tmemcpy(&values[%u], word + %u, %u);\n", j, kls->startpositions[j], kls->hashkeysize / kls->hashway);

				printf("\n\tvalue = ");
				for (uint32 j = 0; j < kls->hashway; j++)
				{
					int tmp = kls->hashway - j - 1;
					printf("(uint%u) values[%u] << %u%s", kls->hashkeysize * 8, tmp, tmp * 8 * (kls->hashkeysize / kls->hashway), j + 1 == kls->hashway ? ";\n" : " | ");
				}
				if (options->CaseInsensitive)
					printf("\tvalue |= %s;\n", AsciiLowerCaseMask[kls->hashkeysize]);
			}

			printf("\tbucketidx = ((value * (uint32) %u) >> %u) %% %u;\n", kls->hashseed, kls->rightshift, kls->nbuckets);

			if (offsetbase > 0)
				printf("\tbucketidx += %u;\n", offsetbase);
			printf("\tkeywordidx = %s_kw_perfecthash[bucketidx];\n", options->prefix);
			if (!options->EmptyHashSlotReturnFirstKeyWord)
			{
				printf("\tif (keywordidx == -1)\n");
				printf("\t\treturn -1; /* no match, slot empty */\n\n");
			}
			if (options->CaseInsensitive)
				printf("\tif (matches_ascii_lowercase_string(word, &%s_kw_string[%s_kw_offsets[keywordidx]], %u))\n", options->prefix, options->prefix, kls->keywordlen);
			else
				printf("\tif (memcmp(word, &%s_kw_string[%s_kw_offsets[keywordidx]], %u) == 0)\n", options->prefix, options->prefix, kls->keywordlen);
			printf("\t\treturn keywordidx; /* match! */\n\n");
			printf("\t/* no match */\n");
			printf("\treturn -1;\n");
		}
		else
		{
			/* When there's only 1 bucket, don't bother with hashing, just compare to the keyword stored in the bucket */
			if (options->CaseInsensitive)
				printf("\tif (matches_ascii_lowercase_string(word, &%s_kw_string[%s_kw_offsets[%u]], %u))\n", options->prefix, options->prefix, lookupbuckets[offsetbase], kls->keywordlen);
			else
				printf("\tif (memcmp(word, &%s_kw_string[%s_kw_offsets[%u]], %u) == 0)\n", options->prefix, options->prefix, lookupbuckets[offsetbase], kls->keywordlen);
			printf("\t\treturn %u; /* match! */\n\n", offsetbase);
			printf("\t/* no match */\n");
			printf("\treturn -1;\n");
		}
		offsetbase +=  kls->nbuckets;
		printf("}\n\n");
	}
}


int main(int argc, char **argv)
{
	uint32 rshift;
	uint32 wordlen;
	uint32 rounds;
	Options options;

	if (argc < 3)
	{
		fprintf(stderr, "Usage: %s <word len> <rounds>\n", argv[0]);
		return -1;
	}

	options.prefix = "ScanKeywords";
	options.NullTerminateKeyWords = true;
	options.EmptyHashSlotReturnFirstKeyWord = true;
	options.ExtraOffsetElement = true;
	options.CaseInsensitive = true;
	options.Verbose = false;

	wordlen = atoi(argv[1]);
	rounds = atoi(argv[2]);

	loadkeywords(options.Verbose);

	if (wordlen != 0)
	{
		uint32 best_nbuckets;

		best_nbuckets = process_word(wordlen, rounds, &options);
	}
	else
	{
		uint32 total_buckets = 0;
		uint32 longest_word = strlen(keywords[nkeywords - 1].keyword);

		/* Process all wordlens */
		for (uint32 wordlen = 1; wordlen <= longest_word; wordlen++)
		{
			int32 best_nbuckets;

			best_nbuckets = process_word(wordlen, rounds, &options);

			if (best_nbuckets == 0)
			{
				printf("Can't hash for wordlen = %u\n", wordlen);
				return -1;
			}
			total_buckets += best_nbuckets;
		}

		print_final_code(&options);
	}

	return 0;
}
