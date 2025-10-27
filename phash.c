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

static uint16
preparekey_2by1(const char *word, uint32 wordlen, uint32 *startpos)
{
	uint16 value;
	memcpy(&value, word + startpos[0], 2);

	return (uint32) value;
}

static uint16
preparekey_2by2(const char *word, uint32 wordlen, uint32 *startpos)
{
	uint8 values[2];
	
	memcpy(&values[0], word + startpos[0], 1);
	memcpy(&values[1], word + startpos[1], 1);
	return (uint16) (values[1]) << 8 | (uint16) values[0];
}

static uint32
preparekey_4by1(const char *word, uint32 wordlen, uint32 *startpos)
{
	uint32 value;
	memcpy(&value, word + startpos[0], 4);

	return value;
}

uint32
preparekey_4by2(const char *word, uint32 wordlen, uint32 *startpos)
{
	uint16 values[2];
	
	memcpy(&values[0], word + startpos[0], 2);
	memcpy(&values[1], word + startpos[1], 2);
	return (uint32) (values[1]) << 16 | (uint32) values[0];
}

uint32
preparekey_4by4(const char *word, uint32 wordlen, uint32 *startpos)
{
	unsigned char values[4];
	
	memcpy(&values[0], word + startpos[0], 1);
	memcpy(&values[1], word + startpos[1], 1);
	memcpy(&values[2], word + startpos[2], 1);
	memcpy(&values[3], word + startpos[3], 1);
	
	return (uint32) (values[3]) << 24 |
		   (uint32) values[2] << 16 |
		   (uint32) values[1] << 8 |
		   (uint32) values[0];
}

static uint64
preparekey_8by1(const char *word, uint32 wordlen, uint32 *startpos)
{
	uint64 value;
	memcpy(&value, word + startpos[0], 8);

	return value;
}

uint64
preparekey_8by2(const char *word, uint32 wordlen, uint32 *startpos)
{
	uint32 values[2];
	
	memcpy(&values[0], word + startpos[0], 4);
	memcpy(&values[1], word + startpos[1], 4);
	return (uint64) (values[1]) << 32 | (uint64) values[0];
}

uint64
preparekey_8by4(const char *word, uint32 wordlen, uint32 *startpos)
{
	uint16 values[4];
	
	memcpy(&values[0], word + startpos[0], 2);
	memcpy(&values[1], word + startpos[1], 2);
	memcpy(&values[2], word + startpos[2], 2);
	memcpy(&values[3], word + startpos[3], 2);
	
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
	uint16 lasthash;

	qsort(wordhashes, numwords, sizeof(uint16), cmp_uint16);

	lasthash = wordhashes[0];
	for (uint32 i = 1; i < numwords; i++)
	{
		if (lasthash == wordhashes[i])
			return 1;
		lasthash = wordhashes[i];
	}
	return 0;
}

static int
has_duplicates32(uint32 *wordhashes, uint32 numwords)
{
	uint32 lasthash;

	qsort(wordhashes, numwords, sizeof(uint32), cmp_uint32);

	lasthash = wordhashes[0];
	for (uint32 i = 1; i < numwords; i++)
	{
		if (lasthash == wordhashes[i])
			return 1;
		lasthash = wordhashes[i];
	}
	return 0;
}

static int
has_duplicates64(uint64 *wordhashes, uint32 numwords)
{
	uint64 lasthash;

	qsort(wordhashes, numwords, sizeof(uint64), cmp_uint64);

	lasthash = wordhashes[0];
	for (uint32 i = 1; i < numwords; i++)
	{
		if (lasthash == wordhashes[i])
			return 1;
		lasthash = wordhashes[i];
	}
	return 0;
}

static void
print_buckets(const char **words, int32 *buckets, uint32 nbuckets)
{
	 for (uint32 i = 0; i < nbuckets; i++)
		printf("%d %s\n", buckets[i], buckets[i] == -1 ? "" : words[buckets[i]]);
}

static void addLookupBucket(int16 value);


#define PH_FUNCNAME searchhash_2by1
#define PH_IDENT "2by1"
#define PH_WAYS 1
#define PH_SHIFTSTART 31
#define PH_SHIFTEND 1
#define PH_HASHTYPE uint16
#define PH_KEYSIZE 2
#include "phash.h"

#define PH_FUNCNAME searchhash_2by2
#define PH_IDENT "2by2"
#define PH_WAYS 2
#define PH_SHIFTSTART 31
#define PH_SHIFTEND 1
#define PH_HASHTYPE uint16
#define PH_KEYSIZE 2
#include "phash.h"

#define PH_FUNCNAME searchhash_4by1
#define PH_IDENT "4by1"
#define PH_WAYS 1
#define PH_SHIFTSTART 31
#define PH_SHIFTEND 1
#define PH_HASHTYPE uint32
#define PH_KEYSIZE 4
#include "phash.h"

#define PH_FUNCNAME searchhash_4by2
#define PH_IDENT "4by2"
#define PH_WAYS 2
#define PH_SHIFTSTART 31
#define PH_SHIFTEND 1
#define PH_HASHTYPE uint32
#define PH_KEYSIZE 4
#include "phash.h"

#define PH_FUNCNAME searchhash_4by4
#define PH_IDENT "4by4"
#define PH_WAYS 4
#define PH_SHIFTSTART 31
#define PH_SHIFTEND 1
#define PH_HASHTYPE uint32
#define PH_KEYSIZE 4
#include "phash.h"

#define PH_FUNCNAME searchhash_8by1
#define PH_IDENT "8by1"
#define PH_WAYS 1
#define PH_SHIFTSTART 63
#define PH_SHIFTEND 1
#define PH_HASHTYPE uint64
#define PH_KEYSIZE 8
#include "phash.h"

#define PH_FUNCNAME searchhash_8by2
#define PH_IDENT "8by2"
#define PH_WAYS 2
#define PH_SHIFTSTART 63
#define PH_SHIFTEND 1
#define PH_HASHTYPE uint64
#define PH_KEYSIZE 8
#include "phash.h"

#define PH_FUNCNAME searchhash_8by4
#define PH_IDENT "8by4"
#define PH_WAYS 4
#define PH_SHIFTSTART 63
#define PH_SHIFTEND 1
#define PH_HASHTYPE uint64
#define PH_KEYSIZE 8
#include "phash.h"

static int
search(KeywordLengthSpecific *kls, Keyword *keywords, uint32 numwords, int32 wordlen, uint32 start_buckets, uint32 max_buckets, uint32 rounds, bool verbose)
{
	uint32 best_nbuckets;
	uint16 *wordhashes16 = aligned_alloc(64, sizeof(uint16) * numwords);
	uint32 *wordhashes32 = aligned_alloc(64, sizeof(uint32) * numwords);
	uint64 *wordhashes64 = aligned_alloc(64, sizeof(uint64) * numwords);
	int32 *buckets = aligned_alloc(64, sizeof(int32) * max_buckets);

	int found = 0;

	/* 2by1 */
	for (int32 startpos = 0; startpos <= wordlen - 2; startpos++)
	{
		for (uint32 i = 0; i < numwords; i++)
			wordhashes16[i] = preparekey_2by1(keywords[i].keyword, wordlen, &startpos);

		if (has_duplicates16(wordhashes16, numwords))
			continue;

		if (verbose)
			printf("Found 2by1 way unique at pos %d ... ", startpos);

		best_nbuckets = searchhash_2by1(kls, keywords, wordhashes16, numwords, wordlen, &startpos, buckets, start_buckets, max_buckets, rounds, verbose);

		if (best_nbuckets != 0)
		{
			if (verbose)
				printf(" found perfect hash\n");
			found = 1;
			goto cleanup;
		}
		printf("Can't find perfect hash for 2by1\n");
	}

	/* 2by2 */
	for (int32 startpos1 = 0; startpos1 <= wordlen - 1; startpos1++)
	{
		for (int32 startpos2 = 0; startpos2 <= wordlen - 1; startpos2++)
		{
			int32 startpos[2] = { startpos1, startpos2 };

			for (uint32 i = 0; i < numwords; i++)
				wordhashes16[i] = preparekey_2by2(keywords[i].keyword, wordlen, startpos);

			if (has_duplicates16(wordhashes16, numwords))
				continue;

			if (verbose)
				printf("Found 2by2 way unique at pos %d %d ... ", startpos[0], startpos[1]);

			best_nbuckets = searchhash_2by2(kls, keywords, wordhashes16, numwords, wordlen, startpos, buckets, start_buckets, max_buckets, rounds, verbose);

			if (best_nbuckets != 0) 
			{
				if (verbose)
					printf(" found perfect hash\n");
				found = 1;
				goto cleanup;
			}
			printf("Can't find perfect hash for 2by2\n");
		}
	}

	if (wordlen >= 2)
	{
		/* 4by1 */
		for (int32 startpos = 0; startpos <= wordlen - 4; startpos++)
		{
			for (uint32 i = 0; i < numwords; i++)
				wordhashes32[i] = preparekey_4by1(keywords[i].keyword, wordlen, &startpos);

			if (has_duplicates32(wordhashes32, numwords))
				continue;
			
			if (verbose)
				printf("Found 4by1 way unique at pos %d ... ", startpos);

			best_nbuckets = searchhash_4by1(kls, keywords, wordhashes32, numwords, wordlen, &startpos, buckets, start_buckets, max_buckets, rounds, verbose);

			if (best_nbuckets != 0) 
			{
				if (verbose)
					printf(" found perfect hash\n");
				found = 1;
				goto cleanup;
			}
			printf("Can't find perfect hash for 4by1\n");
		}

		/* 4by2 */
		for (int32 startpos1 = 0; startpos1 <= wordlen - 2; startpos1++)
		{
			for (int32 startpos2 = 0; startpos2 <= wordlen - 2; startpos2++)
			{
				int32 startpos[2] = { startpos1, startpos2 };

				for (uint32 i = 0; i < numwords; i++)
					wordhashes32[i] = preparekey_4by2(keywords[i].keyword, wordlen, startpos);

				if (has_duplicates32(wordhashes32, numwords))
					continue;
				
				if (verbose)
					printf("Found 4by2 way unique at pos %d %d ... ", startpos[0], startpos[1]);

				best_nbuckets = searchhash_4by2(kls, keywords, wordhashes32, numwords, wordlen, startpos, buckets, start_buckets, max_buckets, rounds, verbose);

				if (best_nbuckets != 0) 
				{
					if (verbose)
						printf(" found perfect hash\n");
					found = 1;
					goto cleanup;
				}
				printf("Can't find perfect hash for 4by2\n");
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
							wordhashes32[i] = preparekey_4by4(keywords[i].keyword, wordlen, startpos);

						if (has_duplicates32(wordhashes32, numwords))
							continue;
						
						if (verbose)
							printf("Found 4by4 way unique at pos %d %d %d %d ... ", startpos[0], startpos[1], startpos[2], startpos[3]);

						best_nbuckets = searchhash_4by4(kls, keywords, wordhashes32, numwords, wordlen, startpos, buckets, start_buckets, max_buckets, rounds, verbose);

						if (best_nbuckets != 0) 
						{
							if (verbose)
								printf(" found perfect hash\n");
							found = 1;
							goto cleanup;
						}
						printf("Can't find perfect hash for 4by4\n");
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
				wordhashes64[i] = preparekey_8by1(keywords[i].keyword, wordlen, &startpos);

			if (has_duplicates64(wordhashes64, numwords))
				continue;
			
			if (verbose)
				printf("Found 8by1 way unique at pos %d ... ", startpos);

			best_nbuckets = searchhash_8by1(kls, keywords, wordhashes64, numwords, wordlen, &startpos, buckets, start_buckets, max_buckets, rounds, verbose);

			if (best_nbuckets != 0) 
			{
				if (verbose) printf(" found perfect hash\n");
				found = 1;
				goto cleanup;
			}
			printf("Can't find perfect hash for 8by1\n");
		}

		/* 8by2 */
		for (int32 startpos1 = 0; startpos1 <= wordlen - 4; startpos1++)
		{
			for (int32 startpos2 = 0; startpos2 <= wordlen - 4; startpos2++)
			{
				int32 startpos[2] = { startpos1, startpos2 };

				for (uint32 i = 0; i < numwords; i++)
					wordhashes64[i] = preparekey_8by2(keywords[i].keyword, wordlen, startpos);

				if (has_duplicates64(wordhashes64, numwords))
					continue;
				
				if (verbose)
					printf("Found 8by2 way unique at pos %d %d ... ", startpos[0], startpos[1]);

				best_nbuckets = searchhash_8by2(kls, keywords, wordhashes64, numwords, wordlen, startpos, buckets, start_buckets, max_buckets, rounds, verbose);

				if (best_nbuckets != 0) 
				{
					if (verbose) printf(" found perfect hash\n");
					found = 1;
					goto cleanup;
				}
				printf("Can't find perfect hash for 8by2\n");
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
							wordhashes64[i] = preparekey_8by4(keywords[i].keyword, wordlen, startpos);

						if (has_duplicates64(wordhashes64, numwords))
							continue;
						
						if (verbose)
							printf("Found 8by4 way unique at pos %d %d %d %d ... ", startpos[0], startpos[1], startpos[2], startpos[3]);

						best_nbuckets = searchhash_8by4(kls, keywords, wordhashes64, numwords, wordlen, startpos, buckets, start_buckets, max_buckets, rounds, verbose);

						if (best_nbuckets != 0) 
						{
							if (verbose)
								printf(" found perfect hash\n");
							found = 1;
							goto cleanup;
						}
						printf("Can't find perfect hash for 8by4\n");
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
	int guess = (int) ((double) n * 1.25);
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
	if (verbose)
		printf("Added word %s at index %u\n", keyword, nkeywords);
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

int loadkeywords(bool verbose)
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

void
print_keywords(void)
{
	for (int i = 0; i < nkeywords; i++)
		printf("%s\n", keywords[i].keyword);
}

int32
process_word(uint32 wordlen, uint32 rounds, bool verbose)
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

	printf("* Num keywords = %u wordlen = %u\n", last - first, wordlen);

	nbuckets = guess_initial_hash_size(last - first);
	kls = addKeywordLengthSpecific();
	best_nbuckets = search(kls, &keywords[first], last - first, wordlen, nbuckets, 1024, rounds, verbose);

	if (best_nbuckets != 0)
		return best_nbuckets;
	else
	{
		fprintf(stderr, "Failed to find perfect hash for wordlen = %u\n", wordlen);
		exit(-1);
	}
}
/*
uint32
preparekey_4by4(const char *word, uint32 wordlen, uint32 *startpos)
{
	unsigned char values[4];
	
	memcpy(&values[0], word + startpos[0], 1);
	memcpy(&values[1], word + startpos[1], 1);
	memcpy(&values[2], word + startpos[2], 1);
	memcpy(&values[3], word + startpos[3], 1);
	
	return (uint32) (values[3]) << 24 |
		   (uint32) values[2] << 16 |
		   (uint32) values[1] << 8 |
		   (uint32) values[0];
}
*/
void
print_final_code(void)
{
	uint32 offsetbase = 0;
	for (uint32 i = 0; i < nlookupbuckets; i++)
	{
		printf("%d,\n",lookupbuckets[i]);
	}

	for (uint32 i = 0; i < nkeywordlengthspecifics; i++)
	{
		KeywordLengthSpecific *kls = &keywordlengthspecific[i];

		printf("int16 wordlookup%u(const char *word)\n", kls->keywordlen);
		printf("{\n");
		printf("\tuint32 bucketidx;\n");
		printf("\tuint%u value;\n", kls->hashkeysize * 8);
		if (kls->hashway == 1)
		{
			printf("\tmemcpy(&value, word + %u, %u);\n\n", kls->startpositions[0], kls->hashkeysize);
		}
		else
		{
			printf("\tuint%u values[%u];\n\n", kls->hashkeysize * 8, kls->hashway);

			for (uint32 j = 0; j < kls->hashway; j++)
				printf("\tmemcpy(&values[%u], word + %u, %u);\n", j, kls->startpositions[j], kls->hashkeysize / kls->hashway);

			printf("\n\tvalue = ");
			for (uint32 j = 0; j < kls->hashway; j++)
			{
				int tmp = kls->hashway - j - 1;
				printf("(uint%u) values[%u] << %u%s", kls->hashkeysize * 8, tmp, tmp * 8 * (kls->hashkeysize / kls->hashway), j + 1 == kls->hashway ? ";\n" : " | ");
			}
		}

		printf("\tbucketidx = ((value * %u) >> %u) %% %u;\n", kls->hashseed, kls->rightshift, kls->nbuckets);
		printf("\tbucketidx += %u;\n", offsetbase);
		printf("\treturn bucketidx;\n");
		offsetbase +=  kls->nbuckets;
		printf("}\n\n");
	}
}


int main(int argc, char **argv)
{
	uint32 rshift;
	uint32 wordlen;
	uint32 rounds;
	bool 	verbose = false;
	
	if (argc < 3)
	{
		fprintf(stderr, "Usage: %s <word len> <rounds>\n", argv[0]);
		return -1;
	}

	wordlen = atoi(argv[1]);
	rounds = atoi(argv[2]);

	loadkeywords(verbose);
	//print_keywords();

	if (wordlen != 0)
	{
		uint32 best_nbuckets;
		
		best_nbuckets = process_word(wordlen, rounds, verbose);
		printf("best nbuckets for wordlen = %u is %u\n", wordlen, best_nbuckets);
	}
	else
	{
		uint32 total_buckets = 0;
		uint32 longest_word = strlen(keywords[nkeywords - 1].keyword);

		/* Process all wordlens */
		for (uint32 wordlen = 1; wordlen <= longest_word; wordlen++)
		{
			int32 best_nbuckets;
			
			best_nbuckets = process_word(wordlen, rounds, verbose);
			
			if (best_nbuckets == 0)
			{
				printf("Can't hash for wordlen = %u\n", wordlen);
				return -1;
			}
			total_buckets += best_nbuckets;
		}

		print_final_code();

		printf("total_buckets = %u\n", total_buckets);
	}

	return 0;
}
