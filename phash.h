/*
#define PH_FUNCNAME searchhash_2by1
#define PH_IDENT "2by1"
#define PH_WAYS 1
#define PH_SHIFTSTART 31
#define PH_SHIFTEND 1
#define PH_HASHTYPE uint16
#define PH_KEYSIZE 2
*/

static inline uint32
PH_FUNCNAME(KeywordLengthSpecific *kls, Keyword *words, PH_HASHTYPE *wordhashes, uint32 numwords, uint32 wordlen, uint32 *startpos, int32 *buckets, uint32 start_buckets, uint32 max_buckets, uint32 rounds, bool verbose)
{
	uint32 best_seed = 0;
	uint32 best_nbuckets = UINT_MAX;
	uint32 best_rshift = 0;
	int32 *best_buckets = malloc(sizeof(int32) * max_buckets);

	for (uint32 r = 0; r < rounds; r++)
	{
		uint32 seed = rand();

		for (uint32 rshift = PH_SHIFTSTART; rshift >= PH_SHIFTEND; rshift--)
		{
			for (uint32 nbuckets = start_buckets; nbuckets < max_buckets; nbuckets++)
			{
				uint32 i;
				memset(buckets, -1, sizeof(int32) * nbuckets);

				for (i = 0; i < numwords; i++)
				{
					const char *word = words[i].keyword;
					PH_HASHTYPE wordhash;
					uint32 bucketidx;
					
					wordhash = wordhashes[i];

					bucketidx = ((wordhash * seed) >> rshift) % nbuckets;
					if (buckets[bucketidx] != -1)
						break;

					buckets[bucketidx] = words[i].kw_index;
				}

				if (i == numwords)
				{
					if (nbuckets < best_nbuckets)
					{
						best_nbuckets = nbuckets;
						best_seed = seed;
						best_rshift = rshift;
						memcpy(best_buckets, buckets, sizeof(int32) * nbuckets);
					}
				}
			}
		}
	}

	if (best_nbuckets == UINT_MAX)
		return 0;

	kls->keywordlen = wordlen;
	kls->hashway = PH_WAYS;
	kls->hashkeysize = PH_KEYSIZE;
	kls->hashseed = best_seed;
	kls->rightshift = best_rshift;
	kls->startpositions[4];
	kls->nbuckets = best_nbuckets;
	memcpy(kls->startpositions, startpos, sizeof(uint32) * PH_WAYS);

	if (verbose)
	{
		if (PH_WAYS == 1)
			printf(PH_IDENT ": found seed %u numwords = %u wordlen = %u rshift = %u nbuckets = %u startpos[0] = %u\n", best_seed, numwords, wordlen, best_rshift, best_nbuckets, startpos[0]);
		else if (PH_WAYS == 2)
			printf(PH_IDENT ": found seed %u numwords = %u wordlen = %u rshift = %u nbuckets = %u startpos[0] = %u startpos[1] = %u\n", best_seed, numwords, wordlen, best_rshift, best_nbuckets, startpos[0], startpos[1]);
		else if (PH_WAYS == 3)
			printf(PH_IDENT ": found seed %u numwords = %u wordlen = %u rshift = %u nbuckets = %u startpos[0] = %u startpos[1] = %u startpos[2] = %u\n", best_seed, numwords, wordlen, best_rshift, best_nbuckets, startpos[0], startpos[1], startpos[2]);
		else if (PH_WAYS == 2)
			printf(PH_IDENT ": found seed %u numwords = %u wordlen = %u rshift = %u nbuckets = %u startpos[0] = %u startpos[1] = %u startpos[2] == %u startpos[3] = %u\n", best_seed, numwords, wordlen, best_rshift, best_nbuckets, startpos[0], startpos[1], startpos[2], startpos[3]);
	}

	for (uint32 i = 0; i < best_nbuckets; i++)
	{
		if (best_buckets[i] == -1)
			addLookupBucket(-1);	/* empty bucket */
		else
			addLookupBucket(best_buckets[i]);
	}

	free(best_buckets);

	return best_nbuckets;
}

#undef PH_FUNCNAME
#undef PH_IDENT
#undef PH_WAYS
#undef PH_SHIFTSTART
#undef PH_SHIFTEND
#undef PH_HASHTYPE
#undef PH_KEYSIZE

