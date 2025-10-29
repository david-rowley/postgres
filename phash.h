/*
#define PH_FUNCNAME searchhash_2by1
#define PH_IDENT "2by1"
#define PH_WAYS 1
#define PH_SHIFTSTART 29
#define PH_SHIFTEND 10
#define PH_HASHTYPE uint16
#define PH_KEYSIZE 2
*/

static inline uint32
PH_FUNCNAME(KeywordLengthSpecific *kls, Keyword *words,
			PH_HASHTYPE *wordhashes, uint32 numwords, uint32 wordlen,
			uint32 *startpos, int32 *buckets, uint32 start_buckets,
			uint32 max_buckets, uint32 rounds, bool verbose)
{
	uint32 best_seed = 0;
	uint32 best_nbuckets = UINT_MAX;
	uint32 best_rshift = 0;
	int32 *best_buckets = malloc(sizeof(int32) * max_buckets);
	PH_HASHTYPE *seededwords = malloc(sizeof(PH_HASHTYPE) * numwords);
	uint32 bloomfilter_size = (max_buckets + 7) / 8;
	uint8 *bloomfilter = malloc(bloomfilter_size);

	/*
	 * We use this array below to record the kw_index and use it to check
	 * for collisions with other words.  When we find a collision we need
	 * to retry with another rshift amount or another seed.  To save from
	 * having to set this to -1 each retry we record the indexes of the
	 * elements we've changed in the 'unset' array and just do those ones.
	 * This seems to help performance
	 */
	memset(buckets, -1, sizeof(int32) * max_buckets);

	/*
	 * Start looking for the smallest number of buckets we can use for this
	 * set of words.
	 */
	for (uint32 r = 0; r < rounds; r++)
	{
		uint32 seed = rand();

		/*
		 * Precalculate the hashes before the bitshift portion so that we
		 * don't have to recalculate this every time in the loop below.
		 */
		for (uint32 w = 0; w < numwords; w++)
			seededwords[w] = (wordhashes[w] * seed);

		for (uint32 nbuckets = start_buckets; nbuckets < max_buckets; nbuckets++)
		{
			bloomfilter_size = (nbuckets + 7) / 8;
			memset(bloomfilter, 0, bloomfilter_size);

			for (uint32 rshift = PH_SHIFTSTART; rshift <= PH_SHIFTEND; rshift++)
			{
				for (uint32 i = 0; i < numwords; i++)
				{
					uint32 bucketidx = (seededwords[i] >> rshift) % nbuckets;
					uint32 bf_idx = bucketidx >> 3;
					uint32 bf_bit = (1 << (bucketidx & 7));

					if (bloomfilter[bf_idx] & bf_bit)
						goto resetbuckets;

					bloomfilter[bf_idx] |= bf_bit;
				}

				/* bloomfilter found no collisions, build the bucket array */
				for (uint32 i = 0; i < numwords; i++)
				{
					uint32 bucketidx = (seededwords[i] >> rshift) % nbuckets;
					buckets[bucketidx] = i;
				}

				if (nbuckets < best_nbuckets)
				{
					/*
					 * If we managed to fit the words in this number of buckets, then no need
					 * to try a larger number of buckets again, in fact, no need to try the
					 * same number again since we'll only accept a new better much if we beat
					 * the last bucket count.
					 */
					max_buckets = nbuckets - 1;

					best_nbuckets = nbuckets;
					best_seed = seed;
					best_rshift = rshift;
					memcpy(best_buckets, buckets, sizeof(int32) * nbuckets);
				}
				memset(buckets, -1, sizeof(int32) * nbuckets);
resetbuckets:
				memset(bloomfilter, 0, bloomfilter_size);
			}
		}
	}

	if (best_nbuckets < UINT_MAX)
	{
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
			else if (PH_WAYS == 4)
				printf(PH_IDENT ": found seed %u numwords = %u wordlen = %u rshift = %u nbuckets = %u startpos[0] = %u startpos[1] = %u startpos[2] == %u startpos[3] = %u\n", best_seed, numwords, wordlen, best_rshift, best_nbuckets, startpos[0], startpos[1], startpos[2], startpos[3]);
		}

		/* record the best buckets for this wordlen in the global bucket array */
		for (uint32 i = 0; i < best_nbuckets; i++)
		{
			if (best_buckets[i] == -1)
				addLookupBucket(-1);
			else
				addLookupBucket(words[best_buckets[i]].kw_index);
		}
	}
	else
	{
		printf(PH_IDENT ": Failed to find perfect hash for wordlen %u\n", wordlen);
		best_nbuckets = 0;
	}

	free(best_buckets);
	free(seededwords);

	return best_nbuckets;
}

#undef PH_FUNCNAME
#undef PH_IDENT
#undef PH_WAYS
#undef PH_SHIFTSTART
#undef PH_SHIFTEND
#undef PH_HASHTYPE
#undef PH_KEYSIZE
