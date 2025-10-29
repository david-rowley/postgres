/*
#define PH_FUNCNAME searchhash_2by1
#define PH_IDENT "2by1"
#define PH_WAYS 1
#define PH_HASHTYPE uint16
#define PH_KEYSIZE 2
*/

static inline uint32
PH_FUNCNAME(KeywordLengthSpecific *kls, Keyword *words,
			PH_HASHTYPE *wordhashes, uint32 numwords, uint32 wordlen,
			uint32 *startpos, int16 *buckets, uint32 start_buckets,
			uint32 max_buckets, uint32 rounds, bool verbose)
{
	uint32 best_seed = 0;
	uint32 best_nbuckets = UINT_MAX;
	uint32 best_rshift = 0;
	int16 *best_buckets = malloc(sizeof(int16) * max_buckets);
	uint32 *seededwords = malloc(sizeof(uint32) * numwords);
	uint32 bloomfilter_size = (max_buckets + 7) / 8;
	uint8 *bloomfilter = malloc(bloomfilter_size);
	SeedGenerator seeder;
	uint32 seed;

	memset(buckets, -1, sizeof(int16) * max_buckets);

	SeedGenerator_Setup(&seeder, rounds);

	/*
	 * Start looking for the smallest number of buckets we can use for this
	 * set of words.
	 */
	while ((seed = SeedGenerator_NextSeed(&seeder)) != 0)
	//for (seed = 1; seed != 10000000; seed++) /* all 32-bit space apart from 0 */
	{
		uint32 seededword_mask = 0;

		/* We won't be able to shrink max_buckets lower than this, so just exit the loop */
		if (max_buckets <= numwords)
			break;
		/*
		 * Precalculate the hashes before the bitshift portion so that we
		 * don't have to recalculate this every time in the loop below.
		 */
		for (uint32 w = 0; w < numwords; w++)
		{
			seededwords[w] = (wordhashes[w] * seed);
			seededword_mask |= seededwords[w];
		}

		for (uint32 nbuckets = start_buckets; nbuckets < max_buckets; nbuckets++)
		{
			bloomfilter_size = (nbuckets + 7) / 8;

			/*
			 * The bloom filter could only have grown by 1 element as we only
			 * increase nbuckets 1 at a time, so just zero the final one in
			 * case it's new.
			 */
			bloomfilter[bloomfilter_size - 1] = 0;

			for (uint32 rshift = 0; rshift <= 31; rshift++)
			{
				/*
				 * Don't bother with this rshift if it causes the
				 * mask of all the seeded hashes to become 0.  This
				 * saves a little work being done in the code below.
				 */
				if (seededword_mask >> rshift == 0)
					continue;

				for (uint32 i = 0; i < numwords; i++)
				{
					uint32 bucketidx = (seededwords[i] >> rshift) % nbuckets;
					uint32 bf_idx = bucketidx >> 3;
					uint32 bf_bit = (1 << (bucketidx & 7));

					/* Check for collisions in the bloom filter. */
					if (bloomfilter[bf_idx] & bf_bit)
						goto resetbloomfilter;

					bloomfilter[bf_idx] |= bf_bit;
				}

				//suitable_hashes_found++;
				/* bloomfilter found no collisions; build the bucket array */
				for (uint32 i = 0; i < numwords; i++)
				{
					uint32 bucketidx = (seededwords[i] >> rshift) % nbuckets;
					buckets[bucketidx] = words[i].kw_index;
				}

				if (nbuckets < best_nbuckets)
				{
					/*
					 * If we managed to fit the words in this number of buckets, then no need
					 * to try a larger number of buckets again, in fact, no need to try the
					 * same number again since we'll only accept a new better match if we beat
					 * the last bucket count.
					 */
					max_buckets = nbuckets - 1;

					best_nbuckets = nbuckets;
					best_seed = seed;
					best_rshift = rshift;
					memcpy(best_buckets, buckets, sizeof(int16) * nbuckets);
				}
				memset(buckets, -1, sizeof(int16) * nbuckets);
resetbloomfilter:
				memset(bloomfilter, 0, bloomfilter_size);
			} /* Try another shift size */
		} /* Try bigger bucket count */
	} /* Another seed */

	if (best_nbuckets < UINT_MAX)
	{
		kls->keywordlen = wordlen;
		kls->hashway = PH_WAYS;
		kls->hashkeysize = PH_KEYSIZE;
		kls->hashseed = best_seed;
		kls->rightshift = best_rshift;
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
			addLookupBucket(best_buckets[i]);
	}
	else
	{
		printf(PH_IDENT ": Failed to find perfect hash for wordlen %u\n", wordlen);
		best_nbuckets = 0;
	}

	free(best_buckets);
	free(seededwords);
	free(bloomfilter);

	return best_nbuckets;
}

#undef PH_FUNCNAME
#undef PH_IDENT
#undef PH_WAYS
#undef PH_SHIFTSTART
#undef PH_SHIFTEND
#undef PH_HASHTYPE
#undef PH_KEYSIZE
