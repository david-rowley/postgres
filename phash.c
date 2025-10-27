#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static const char *letter2[] = {
 "as",
 "at",
 "by",
 "do",
 "if",
 "in",
 "is",
 "no",
 "of",
 "on",
 "or",
 "to"
}; // found seed -2147483575 rshift = 10 size = 16 (real    0m0.004s)

static const char *letter3[] = {
 "add",
 "all",
 "and",
 "any",
 "asc",
 "bit",
 "csv",
 "day",
 "dec",
 "end",
 "for",
 "int",
 "key",
 "new",
 "nfc",
 "nfd",
 "not",
 "off",
 "old",
 "out",
 "ref",
 "row",
 "set",
 "sql",
 "xml",
 "yes"
}; // found seed -2147205267 rshift = 23 size = 32 (real    0m0.308s)

static const char *letter4[] = {
 "also",
 "both",
 "call",
 "case",
 "cast",
 "char",
 "copy",
 "cost",
 "cube",
 "data",
 "desc",
 "drop",
 "each",
 "else",
 "enum",
 "from",
 "full",
 "hold",
 "hour",
 "into",
 "join",
 "json",
 "keep",
 "keys",
 "last",
 "left",
 "like",
 "load",
 "lock",
 "mode",
 "move",
 "name",
 "next",
 "nfkc",
 "nfkd",
 "none",
 "null",
 "oids",
 "omit",
 "only",
 "over",
 "path",
 "plan",
 "read",
 "real",
 "role",
 "rows",
 "rule",
 "sets",
 "show",
 "skip",
 "some",
 "temp",
 "text",
 "then",
 "ties",
 "time",
 "trim",
 "true",
 "type",
 "user",
 "view",
 "when",
 "with",
 "work",
 "year",
 "zone"
}; // found seed 1529091547 rshift = 24 size = 128 wordstartpos = 0

static const char *letter5[] = {
 "abort",
 "admin",
 "after",
 "alter",
 "array",
 "begin",
 "cache",
 "chain",
 "check",
 "class",
 "close",
 "cross",
 "cycle",
 "depth",
 "empty",
 "error",
 "event",
 "false",
 "fetch",
 "first",
 "float",
 "force",
 "grant",
 "group",
 "ilike",
 "index",
 "inner",
 "inout",
 "input",
 "label",
 "large",
 "least",
 "level",
 "limit",
 "local",
 "match",
 "merge",
 "month",
 "names",
 "nchar",
 "nulls",
 "order",
 "outer",
 "owned",
 "owner",
 "plans",
 "prior",
 "quote",
 "range",
 "reset",
 "right",
 "setof",
 "share",
 "start",
 "stdin",
 "strip",
 "sysid",
 "table",
 "treat",
 "types",
 "union",
 "until",
 "using",
 "valid",
 "value",
 "views",
 "where",
 "write",
 "xmlpi"
}; // found seed 2145959380 rshift = 24 size = 256 wordstartpos = 1

static const char *letter6[] = {
 "absent",
 "access",
 "action",
 "always",
 "atomic",
 "attach",
 "before",
 "bigint",
 "binary",
 "called",
 "column",
 "commit",
 "create",
 "cursor",
 "delete",
 "detach",
 "domain",
 "double",
 "enable",
 "escape",
 "except",
 "exists",
 "family",
 "filter",
 "format",
 "freeze",
 "global",
 "groups",
 "having",
 "header",
 "ignore",
 "import",
 "indent",
 "inline",
 "insert",
 "isnull",
 "listen",
 "locked",
 "logged",
 "method",
 "minute",
 "nested",
 "notify",
 "nowait",
 "nullif",
 "object",
 "offset",
 "option",
 "others",
 "parser",
 "period",
 "policy",
 "quotes",
 "rename",
 "return",
 "revoke",
 "rollup",
 "scalar",
 "schema",
 "scroll",
 "search",
 "second",
 "select",
 "server",
 "simple",
 "source",
 "stable",
 "stdout",
 "stored",
 "strict",
 "string",
 "system",
 "tables",
 "target",
 "unique",
 "update",
 "vacuum",
 "values",
 "window",
 "within"
}; // found seed 2147461649 rshift = 28 size = 256 (real    76m27.769s)
 // found seed 2143971343 rshift = 24 size = 256 wordstartpos = 1
 // found seed 2143971343 rshift = 24 size = 256 wordstartpos = 1
 
static const char *letter7[] = {
 "analyse",
 "analyze",
 "between",
 "boolean",
 "breadth",
 "cascade",
 "catalog",
 "cluster",
 "collate",
 "columns",
 "comment",
 "content",
 "current",
 "decimal",
 "declare",
 "default",
 "definer",
 "depends",
 "disable",
 "discard",
 "exclude",
 "execute",
 "explain",
 "extract",
 "foreign",
 "forward",
 "granted",
 "handler",
 "include",
 "indexes",
 "inherit",
 "instead",
 "integer",
 "invoker",
 "lateral",
 "leading",
 "mapping",
 "matched",
 "natural",
 "nothing",
 "notnull",
 "numeric",
 "objects",
 "options",
 "overlay",
 "partial",
 "passing",
 "placing",
 "prepare",
 "primary",
 "program",
 "refresh",
 "reindex",
 "release",
 "replace",
 "replica",
 "respect",
 "restart",
 "returns",
 "routine",
 "schemas",
 "session",
 "similar",
 "storage",
 "support",
 "trigger",
 "trusted",
 "uescape",
 "unknown",
 "varchar",
 "varying",
 "verbose",
 "version",
 "virtual",
 "without",
 "wrapper",
 "xmlroot"
};

static const char *letter8[] = {
 "absolute",
 "backward",
 "cascaded",
 "coalesce",
 "comments",
 "conflict",
 "continue",
 "database",
 "defaults",
 "deferred",
 "distinct",
 "document",
 "encoding",
 "enforced",
 "external",
 "finalize",
 "function",
 "greatest",
 "grouping",
 "identity",
 "implicit",
 "inherits",
 "interval",
 "language",
 "location",
 "maxvalue",
 "minvalue",
 "national",
 "operator",
 "overlaps",
 "parallel",
 "password",
 "position",
 "prepared",
 "preserve",
 "reassign",
 "relative",
 "restrict",
 "rollback",
 "routines",
 "security",
 "sequence",
 "smallint",
 "snapshot",
 "template",
 "trailing",
 "truncate",
 "unlisten",
 "unlogged",
 "validate",
 "variadic",
 "volatile",
 "xmlparse",
 "xmltable"
}; // found seed 2147360209 rshift = 24 size = 128 wordstartpos = 0

static const char *letter9[] = {
 "aggregate",
 "assertion",
 "attribute",
 "character",
 "collation",
 "committed",
 "delimiter",
 "encrypted",
 "excluding",
 "exclusive",
 "extension",
 "following",
 "functions",
 "generated",
 "immediate",
 "immutable",
 "including",
 "increment",
 "initially",
 "intersect",
 "isolation",
 "leakproof",
 "localtime",
 "normalize",
 "parameter",
 "partition",
 "preceding",
 "precision",
 "procedure",
 "recursive",
 "returning",
 "savepoint",
 "sequences",
 "statement",
 "substring",
 "symmetric",
 "temporary",
 "timestamp",
 "transform",
 "unbounded",
 "validator",
 "xmlconcat",
 "xmlexists",
 "xmlforest"
};

static const char *letter10[] = {
 "asensitive",
 "assignment",
 "asymmetric",
 "checkpoint",
 "connection",
 "constraint",
 "conversion",
 "deallocate",
 "deferrable",
 "delimiters",
 "dictionary",
 "expression",
 "json_array",
 "json_query",
 "json_table",
 "json_value",
 "normalized",
 "ordinality",
 "overriding",
 "privileges",
 "procedural",
 "procedures",
 "references",
 "repeatable",
 "standalone",
 "statistics",
 "tablespace",
 "whitespace",
 "xmlelement",
};

static const char *letter11[] = {
 "compression",
 "conditional",
 "constraints",
 "insensitive",
 "json_exists",
 "json_object",
 "json_scalar",
 "publication",
 "referencing",
 "system_user",
 "tablesample",
 "transaction",
 "uncommitted",
 "unencrypted"
 }; // found seed 2147474698 rshift = 24 size = 16 wordstartpos = 2

static const char *letter12[] = {
 "concurrently",
 "current_date",
 "current_role",
 "current_time",
 "current_user",
 "materialized",
 "merge_action",
 "serializable",
 "session_user",
 "subscription",
 "xmlserialize"
 }; // found seed 2147457174 rshift = 24 size = 16 wordstartpos = 6

static const char *letter13[] = {
 "authorization",
 "configuration",
 "json_arrayagg",
 "unconditional",
 "xmlattributes",
 "xmlnamespaces"
 }; // found seed 2147483535 rshift = 24 size = 8 wordstartpos = 0

static const char *letter14[] = {
 "current_schema",
 "json_objectagg",
 "json_serialize",
 "localtimestamp"
}; // found seed 2147483560 rshift = 24 size = 4 wordstartpos = 3

static const char *letter15[] = {
 "characteristics",
 "current_catalog"
}; // found seed 2147483643 rshift = 24 size = 2 wordstartpos = 0

static const char *letter17[] = {
 "current_timestamp"
}; // found seed 2147483647 rshift = 24 size = 1 wordstartpos = 0

static const char **letters[] = {
	[2] = letter2,
	[3] = letter3,
	[4] = letter4,
	[5] = letter5,
	[6] = letter6,
	[7] = letter7,
	[8] = letter8,
	[9] = letter9,
	[10] = letter10,
	[11] = letter11,
	[12] = letter12,
	[13] = letter13,
	[14] = letter14,
	[15] = letter15,
	[17] = letter17
};

#define lengthof(array) (sizeof (array) / sizeof ((array)[0]))

typedef unsigned long long uint64;
typedef long long int64;
typedef unsigned int uint32;
typedef int int32;
typedef unsigned short uint16;
typedef short int16;
typedef unsigned char uint8;
typedef char int8;

static const int32 letterslen[] = {
	[2] = lengthof(letter2),
	[3] = lengthof(letter3),
	[4] = lengthof(letter4),
	[5] = lengthof(letter5),
	[6] = lengthof(letter6),
	[7] = lengthof(letter7),
	[8] = lengthof(letter8),
	[9] = lengthof(letter9),
	[10] = lengthof(letter10),
	[11] = lengthof(letter11),
	[12] = lengthof(letter12),
	[13] = lengthof(letter13),
	[14] = lengthof(letter14),
	[15] = lengthof(letter15),
	[17] = lengthof(letter17)
};

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
cmp_uint32(const void *pa, const void *pb)
{
	uint32 a = *(uint32 *) pa;
	uint32 b = *(uint32 *) pb;

	return a < b ? -1 : a > b ? +1 : 0;
}

static int
has_duplicates(uint32 *wordhashes, uint32 numwords)
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

static void
print_buckets(const char **words, int32 *buckets, uint32 nbuckets)
{
	// for (uint32 i = 0; i < nbuckets; i++)
		// printf("%d %s\n", buckets[i], buckets[i] == -1 ? "" : words[buckets[i]]);
}

static inline uint32
searchhash_2by1(const char **words, uint32 numwords, uint32 wordlen, uint32 *startpos, int32 *buckets, uint32 nbuckets, uint32 rounds)
{
	uint32 hashmask = nbuckets - 1;

	for (uint32 r = 0; r < rounds; r++)
	{
		uint32 seed = rand();

		for (int rshift = 31; rshift > 0; rshift--)
		{
			uint32 i;

			memset(buckets, -1, sizeof(int32) * nbuckets);

			for (i = 0; i < numwords; i++)
			{
				const char *word = words[i];
				uint16 wordhash ;
				uint32 bucketidx;
				
				wordhash = preparekey_2by1(word, wordlen, startpos);

				bucketidx = ((wordhash * seed) >> rshift) & hashmask;
				if (buckets[bucketidx] != -1)
					break;

				buckets[bucketidx] = i;
			}

			if (i == numwords)
			{
				printf("2by1: found seed %u numwords = %u wordlen = %u rshift = %u nbuckets = %u startpos[0] = %u\n", seed, numwords, wordlen, rshift, nbuckets, startpos[0]);
				print_buckets(words, buckets, nbuckets);
				return 1;
			}
		}
	}
	return 0;
}

static inline uint32
searchhash_2by2(const char **words, uint32 numwords, uint32 wordlen, uint32 *startpos, int32 *buckets, uint32 nbuckets, uint32 rounds)
{
	uint32 hashmask = nbuckets - 1;

	for (uint32 r = 0; r < rounds; r++)
	{
		uint32 seed = rand();
		for (int rshift = 31; rshift > 0; rshift--)
		{
			uint32 i;
		
			memset(buckets, -1, sizeof(int32) * nbuckets);

			for (i = 0; i < numwords; i++)
			{
				const char *word = words[i];
				uint16 wordhash ;
				uint32 bucketidx;
				
				wordhash = preparekey_2by2(word, wordlen, startpos);

				bucketidx = ((wordhash * seed) >> rshift) & hashmask;
				if (buckets[bucketidx] != -1)
					break;

				buckets[bucketidx] = i;
			}

			if (i == numwords)
			{
				printf("2by2: found seed %u numwords = %u wordlen = %u rshift = %u nbuckets = %u startpos[0] = %u, startpos[1] = %u\n", seed, numwords, wordlen, rshift, nbuckets, startpos[0], startpos[1]);
				print_buckets(words, buckets, nbuckets);
				return 1;
			}
		}
	}
	return 0;
}

static inline uint32
searchhash_4by1(const char **words, uint32 numwords, uint32 wordlen, uint32 *startpos, int32 *buckets, uint32 nbuckets, uint32 rounds)
{
	uint32 hashmask = nbuckets - 1;

	for (uint32 r = 0; r < rounds; r++)
	{
		uint32 seed = rand();
		for (int rshift = 31; rshift > 0; rshift--)
		{
			uint32 i;
		
			memset(buckets, -1, sizeof(int32) * nbuckets);

			for (i = 0; i < numwords; i++)
			{
				const char *word = words[i];
				uint32 wordhash ;
				uint32 bucketidx;
				
				wordhash = preparekey_4by1(word, wordlen, startpos);

				bucketidx = ((wordhash * seed) >> rshift) & hashmask;
				if (buckets[bucketidx] != -1)
					break;

				buckets[bucketidx] = i;
			}

			if (i == numwords)
			{
				printf("4by1: found seed %u numwords = %u wordlen = %u rshift = %u nbuckets = %u startpos[0] = %u\n", seed, numwords, wordlen, rshift, nbuckets, startpos[0]);
				print_buckets(words, buckets, nbuckets);
				return 1;
			}
		}
	}
	return 0;
}

static inline uint32
searchhash_4by2(const char **words, uint32 numwords, uint32 wordlen, uint32 *startpos, int32 *buckets, uint32 nbuckets, uint32 rounds)
{
	uint32 hashmask = nbuckets - 1;

	for (uint32 r = 0; r < rounds; r++)
	{
		uint32 seed = rand();
		for (int rshift = 31; rshift > 0; rshift--)
		{
			uint32 i;

			memset(buckets, -1, sizeof(int32) * nbuckets);

			for (i = 0; i < numwords; i++)
			{
				const char *word = words[i];
				uint32 wordhash ;
				uint32 bucketidx;
				
				wordhash = preparekey_4by2(word, wordlen, startpos);

				bucketidx = ((wordhash * seed) >> rshift) & hashmask;
				if (buckets[bucketidx] != -1)
					break;

				buckets[bucketidx] = i;
			}

			if (i == numwords)
			{
				printf("4by2: found seed %u numwords = %u wordlen = %u rshift = %u nbuckets = %u startpos[0] = %u, startpos[1] = %u\n", seed, numwords, wordlen, rshift, nbuckets, startpos[0], startpos[1]);
				print_buckets(words, buckets, nbuckets);
				return 1;
			}
		}
	}
	return 0;
}

static inline uint32
searchhash_4by4(const char **words, uint32 numwords, uint32 wordlen, uint32 *startpos, int32 *buckets, uint32 nbuckets, uint32 rounds)
{
	uint32 hashmask = nbuckets - 1;

	for (uint32 r = 0; r < rounds; r++)
	{
		uint32 seed = rand();
		for (int rshift = 31; rshift > 0; rshift--)
		{
			uint32 i;

			memset(buckets, -1, sizeof(int32) * nbuckets);

			for (i = 0; i < numwords; i++)
			{
				const char *word = words[i];
				uint32 wordhash ;
				uint32 bucketidx;
				
				wordhash = preparekey_4by4(word, wordlen, startpos);

				bucketidx = ((wordhash * seed) >> rshift) & hashmask;
				if (buckets[bucketidx] != -1)
					break;

				buckets[bucketidx] = i;
			}
			if (i == numwords)
			{
				printf("4by4: found seed %u numwords = %u wordlen = %u rshift = %u nbuckets = %u startpos[0] = %u, startpos[1] = %u, startpos[2] = %u, startpos[3] = %u\n", seed, numwords, wordlen, rshift, nbuckets, startpos[0], startpos[1], startpos[2], startpos[3]);
				print_buckets(words, buckets, nbuckets);
				return 1;
			}
		}
	}
	return 0;
}

static inline uint32
searchhash_8by1(const char **words, uint32 numwords, uint32 wordlen, uint32 *startpos, int32 *buckets, uint32 nbuckets, uint32 rounds)
{
	uint32 hashmask = nbuckets - 1;

	for (uint32 r = 0; r < rounds; r++)
	{
		uint32 seed = rand();
		for (int rshift = 63; rshift > 0; rshift--)
		{
			uint32 i;

			memset(buckets, -1, sizeof(int32) * nbuckets);

			for (i = 0; i < numwords; i++)
			{
				const char *word = words[i];
				uint64 wordhash ;
				uint32 bucketidx;
				
				wordhash = preparekey_8by1(word, wordlen, startpos);

				bucketidx = ((wordhash * seed) >> rshift) & hashmask;
				if (buckets[bucketidx] != -1)
					break;

				buckets[bucketidx] = i;
			}

			if (i == numwords)
			{
				printf("8by1: found seed %u numwords = %u wordlen = %u rshift = %u nbuckets = %u startpos[0] = %u\n", seed, numwords, wordlen, rshift, nbuckets, startpos[0]);
				print_buckets(words, buckets, nbuckets);
				return 1;
			}
		}
	}
	return 0;
}

static inline uint32
searchhash_8by2(const char **words, uint32 numwords, uint32 wordlen, uint32 *startpos, int32 *buckets, uint32 nbuckets, uint32 rounds)
{
	uint32 hashmask = nbuckets - 1;

	for (uint32 r = 0; r < rounds; r++)
	{
		uint32 seed = rand();
		for (int rshift = 63; rshift > 0; rshift--)
		{
			uint32 i;

			memset(buckets, -1, sizeof(int32) * nbuckets);

			for (i = 0; i < numwords; i++)
			{
				const char *word = words[i];
				uint64 wordhash;
				uint32 bucketidx;
				
				wordhash = preparekey_8by2(word, wordlen, startpos);

				bucketidx = ((wordhash * seed) >> rshift) & hashmask;
				if (buckets[bucketidx] != -1)
					break;

				buckets[bucketidx] = i;
			}

			if (i == numwords)
			{
				printf("8by2: found seed %u numwords = %u wordlen = %u rshift = %u nbuckets = %u startpos[0] = %u, startpos[1] = %u\n", seed, numwords, wordlen, rshift, nbuckets, startpos[0], startpos[1]);
				print_buckets(words, buckets, nbuckets);
				return 1;
			}
		}
	}
	return 0;
}


static inline uint32
searchhash_8by4(const char **words, uint32 numwords, uint32 wordlen, uint32 *startpos, int32 *buckets, uint32 nbuckets, uint32 rounds)
{
	uint32 hashmask = nbuckets - 1;

	for (uint32 r = 0; r < rounds; r++)
	{
		uint32 seed = rand();
		for (int rshift = 63; rshift > 0; rshift--)
		{
			uint32 i;

			memset(buckets, -1, sizeof(int32) * nbuckets);

			for (i = 0; i < numwords; i++)
			{
				const char *word = words[i];
				uint64 wordhash;
				uint32 bucketidx;
				
				wordhash = preparekey_8by4(word, wordlen, startpos);

				bucketidx = ((wordhash * seed) >> rshift) & hashmask;
				if (buckets[bucketidx] != -1)
					break;

				buckets[bucketidx] = i;
			}
			if (i == numwords)
			{
				printf("8by4: found seed %u numwords = %u wordlen = %u rshift = %u nbuckets = %u startpos[0] = %u, startpos[1] = %u, startpos[2] = %u, startpos[3] = %u\n", seed, numwords, wordlen, rshift, nbuckets, startpos[0], startpos[1], startpos[2], startpos[3]);
				print_buckets(words, buckets, nbuckets);
				return 1;
			}
		}
	}
	return 0;
}

static int
search(const char **words, uint32 numwords, int32 wordlen, uint32 nbuckets, uint32 rounds, bool verbose)
{
	uint32 *wordhashes = aligned_alloc(64, sizeof(uint32) * numwords);
	int32 *buckets = aligned_alloc(64, sizeof(int32) * nbuckets);
	int found = 0;

	/* 2by1 */
	for (int32 startpos = 0; startpos <= wordlen - 2; startpos++)
	{
		memset(wordhashes, 0, sizeof(uint32) * numwords);

		for (uint32 i = 0; i < numwords; i++)
			wordhashes[i] = preparekey_2by1(words[i], wordlen, &startpos);

		if (has_duplicates(wordhashes, numwords))
			continue;

		if (verbose) printf("Found 2by1 way unique at pos %d ... ", startpos);
		if (searchhash_2by1(words, numwords, wordlen, &startpos, buckets, nbuckets, rounds))
		{
			if (verbose) printf(" found perfect hash\n");
			found = 1;
			goto cleanup;
		}
		if (verbose) printf("Can't find perfect hash for 2by1\n");
	}

	/* 2by2 */
	for (int32 startpos1 = 0; startpos1 <= wordlen - 1; startpos1++)
	{
		for (int32 startpos2 = 0; startpos2 <= wordlen - 1; startpos2++)
		{
			int32 startpos[2] = { startpos1, startpos2 };
			memset(wordhashes, 0, sizeof(uint32) * numwords);

			for (uint32 i = 0; i < numwords; i++)
				wordhashes[i] = preparekey_2by2(words[i], wordlen, startpos);

			if (has_duplicates(wordhashes, numwords))
				continue;
			
			if (verbose) printf("Found 2by2 way unique at pos %d %d ... ", startpos[0], startpos[1]);
			if (searchhash_4by2(words, numwords, wordlen, startpos, buckets, nbuckets, rounds))
			{
				if (verbose) printf(" found perfect hash\n");
				found = 1;
				goto cleanup;
			}
			if (verbose) printf("Can't find perfect hash for 2by2\n");
		}
	}

	if (wordlen >= 2)
	{
		/* 4by1 */
		for (int32 startpos = 0; startpos <= wordlen - 4; startpos++)
		{
			memset(wordhashes, 0, sizeof(uint32) * numwords);

			for (uint32 i = 0; i < numwords; i++)
				wordhashes[i] = preparekey_4by1(words[i], wordlen, &startpos);

			if (has_duplicates(wordhashes, numwords))
				continue;
			
			if (verbose) printf("Found 4by1 way unique at pos %d ... ", startpos);
			if (searchhash_4by1(words, numwords, wordlen, &startpos, buckets, nbuckets, rounds))
			{
				if (verbose) printf(" found perfect hash\n");
				found = 1;
				goto cleanup;
			}
			if (verbose) printf("Can't find perfect hash for 4by1\n");
		}

		/* 4by2 */
		for (int32 startpos1 = 0; startpos1 <= wordlen - 2; startpos1++)
		{
			for (int32 startpos2 = 0; startpos2 <= wordlen - 2; startpos2++)
			{
				int32 startpos[2] = { startpos1, startpos2 };
				memset(wordhashes, 0, sizeof(uint32) * numwords);

				for (uint32 i = 0; i < numwords; i++)
					wordhashes[i] = preparekey_4by2(words[i], wordlen, startpos);

				if (has_duplicates(wordhashes, numwords))
					continue;
				
				if (verbose) printf("Found 4by2 way unique at pos %d %d ... ", startpos[0], startpos[1]);
				if (searchhash_4by2(words, numwords, wordlen, startpos, buckets, nbuckets, rounds))
				{
					if (verbose) printf(" found perfect hash\n");
					found = 1;
					goto cleanup;
				}
				if (verbose) printf("Can't find perfect hash for 4by2\n");
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
						memset(wordhashes, 0, sizeof(uint32) * numwords);

						for (uint32 i = 0; i < numwords; i++)
							wordhashes[i] = preparekey_4by4(words[i], wordlen, startpos);

						if (has_duplicates(wordhashes, numwords))
							continue;
						
						if (verbose) printf("Found 4by4 way unique at pos %d %d %d %d ... ", startpos[0], startpos[1], startpos[2], startpos[3]);
						if (searchhash_4by4(words, numwords, wordlen, startpos, buckets, nbuckets, rounds))
						{
							if (verbose) printf(" found perfect hash\n");
							found = 1;
							goto cleanup;
						}
						if (verbose) printf("Can't find perfect hash for 4by4\n");
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
			memset(wordhashes, 0, sizeof(uint32) * numwords);

			for (uint32 i = 0; i < numwords; i++)
				wordhashes[i] = preparekey_8by1(words[i], wordlen, &startpos);

			if (has_duplicates(wordhashes, numwords))
				continue;
			
			if (verbose) printf("Found 8by1 way unique at pos %d ... ", startpos);
			if (searchhash_8by1(words, numwords, wordlen, &startpos, buckets, nbuckets, rounds))
			{
				if (verbose) printf(" found perfect hash\n");
				found = 1;
				goto cleanup;
			}
			if (verbose) printf("Can't find perfect hash for 8by1\n");
		}

		/* 8by2 */
		for (int32 startpos1 = 0; startpos1 <= wordlen - 4; startpos1++)
		{
			for (int32 startpos2 = 0; startpos2 <= wordlen - 4; startpos2++)
			{
				int32 startpos[2] = { startpos1, startpos2 };
				memset(wordhashes, 0, sizeof(uint32) * numwords);

				for (uint32 i = 0; i < numwords; i++)
					wordhashes[i] = preparekey_8by2(words[i], wordlen, startpos);

				if (has_duplicates(wordhashes, numwords))
					continue;
				
				if (verbose) printf("Found 8by2 way unique at pos %d %d ... ", startpos[0], startpos[1]);
				if (searchhash_8by2(words, numwords, wordlen, startpos, buckets, nbuckets, rounds))
				{
					if (verbose) printf(" found perfect hash\n");
					found = 1;
					goto cleanup;
				}
				if (verbose) printf("Can't find perfect hash for 8by2\n");
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
						memset(wordhashes, 0, sizeof(uint32) * numwords);

						for (uint32 i = 0; i < numwords; i++)
							wordhashes[i] = preparekey_8by4(words[i], wordlen, startpos);

						if (has_duplicates(wordhashes, numwords))
							continue;
						
						if (verbose) printf("Found 8by4 way unique at pos %d %d %d %d ... ", startpos[0], startpos[1], startpos[2], startpos[3]);
						if (searchhash_8by4(words, numwords, wordlen, startpos, buckets, nbuckets, rounds))
						{
							if (verbose) printf(" found perfect hash\n");
							found = 1;
							goto cleanup;
						}
						if (verbose) printf("Can't find perfect hash for 8by4\n");
					}
				}
			}
		}
	}

cleanup:
	free(buckets);
	free(wordhashes);

	return found;
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

uint32
process_word(uint32 wordlen, uint32 rounds, bool verbose)
{
	uint32 nbuckets = guess_initial_hash_size(letterslen[wordlen]);

	printf("* Num words = %u\n", letterslen[wordlen]);

	for (; nbuckets <= 1024; nbuckets *= 2)
	{
		printf("* Trying nbuckets = %u\n", nbuckets);
		if (search(letters[wordlen], letterslen[wordlen], wordlen, nbuckets, rounds, verbose))
			return nbuckets;
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

	if (wordlen != 0 && (wordlen < 1 || wordlen > lengthof(letters) || letters[wordlen] == NULL))
	{
		fprintf(stderr, "Invalid wordlen\n");
		return -1;
	}

	if (wordlen != 0)
		process_word(wordlen, rounds, verbose);
	else
	{
		uint32 total_buckets = 0;
		/* Process all wordlens */
		for (uint32 wordlen = 0; wordlen < lengthof(letters); wordlen++)
		{
			uint32 bucket;
			
			if (letters[wordlen] == NULL)
				continue;
			
			bucket = process_word(wordlen, rounds, verbose);
			
			if (bucket == 0)
			{
				printf("Can't hash for wordlen = %u\n", wordlen);
				return -1;
			}
			total_buckets += bucket;
		}
		printf("total_buckets = %u\n", total_buckets);
	}

	return 0;
}
