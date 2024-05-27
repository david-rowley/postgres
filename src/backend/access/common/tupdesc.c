/*-------------------------------------------------------------------------
 *
 * tupdesc.c
 *	  POSTGRES tuple descriptor support code
 *
 * Portions Copyright (c) 1996-2024, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/common/tupdesc.c
 *
 * NOTES
 *	  some of the executor utility code such as "ExecTypeFromTL" should be
 *	  moved here.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/htup_details.h"
#include "access/toast_compression.h"
#include "access/tupdesc_details.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "common/hashfn.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/resowner.h"
#include "utils/syscache.h"

/* ResourceOwner callbacks to hold tupledesc references  */
static void ResOwnerReleaseTupleDesc(Datum res);
static char *ResOwnerPrintTupleDesc(Datum res);

static const ResourceOwnerDesc tupdesc_resowner_desc =
{
	.name = "tupdesc reference",
	.release_phase = RESOURCE_RELEASE_AFTER_LOCKS,
	.release_priority = RELEASE_PRIO_TUPDESC_REFS,
	.ReleaseResource = ResOwnerReleaseTupleDesc,
	.DebugPrint = ResOwnerPrintTupleDesc
};

/* Convenience wrappers over ResourceOwnerRemember/Forget */
static inline void
ResourceOwnerRememberTupleDesc(ResourceOwner owner, TupleDesc tupdesc)
{
	ResourceOwnerRemember(owner, PointerGetDatum(tupdesc), &tupdesc_resowner_desc);
}

static inline void
ResourceOwnerForgetTupleDesc(ResourceOwner owner, TupleDesc tupdesc)
{
	ResourceOwnerForget(owner, PointerGetDatum(tupdesc), &tupdesc_resowner_desc);
}

/*
 * CreateTemplateTupleDesc
 *		This function allocates an empty tuple descriptor structure.
 *
 * Tuple type ID information is initially set for an anonymous record type;
 * caller can overwrite this if needed.
 */
TupleDesc
CreateTemplateTupleDesc(int natts)
{
	TupleDesc	desc;

	/*
	 * sanity checks
	 */
	Assert(natts >= 0);

	/*
	 * Allocate enough memory for the tuple descriptor, including the
	 * attribute rows.
	 *
	 * Note: the attribute array stride is sizeof(TupleDescAttr),
	 * since we declare the array elements as TupleDescAttr for
	 * notational convenience.
	 */
	desc = (TupleDesc) palloc(offsetof(struct TupleDescData, attrs) +
							  natts * sizeof(TupleDescAttr));

	/*
	 * Initialize other fields of the tupdesc.
	 */
	desc->natts = natts;
	desc->extra = (TupleDescExtra *) palloc(offsetof(struct TupleDescExtra, attrs) +
											natts + sizeof(TupleDescAttrExtra));
	desc->tdtypeid = RECORDOID;
	desc->tdtypmod = -1;
	desc->tdrefcount = -1;		/* assume not reference-counted */

	return desc;
}

/*
 * CreateTupleDesc
 *		This function allocates a new TupleDesc by copying a given
 *		Form_pg_attribute array.
 *
 * Tuple type ID information is initially set for an anonymous record type;
 * caller can overwrite this if needed.
 */
TupleDesc
CreateTupleDesc(int natts, Form_pg_attribute *attrs)
{
	TupleDesc	desc;
	TupleDescExtra *extra;
	int			i;

	desc = CreateTemplateTupleDesc(natts);

	for (i = 0; i < natts; ++i)
	{
		TupleDescAttr *attr = TupleDescAttr(desc, i);

		attr->attcacheoff = attrs[i]->attcacheoff;
		attr->attlen = attrs[i]->attlen;
		attr->attbyval = attrs[i]->attbyval;
		attr->attalign = attrs[i]->attalign;
	}

	extra = desc->extra;

	for (i = 0; i < natts; ++i)
	{
		TupleDescAttrExtra *attr = TupleDescExtraAttr(extra, i);

		attr->attrelid = attrs[i]->attrelid;
		memcpy(NameStr(attr->attname), NameStr(attrs[i]->attname), NAMEDATALEN);
		attr->atttypid = attrs[i]->atttypid;
		attr->attnum = attrs[i]->attnum;
		attr->atttypmod = attrs[i]->atttypmod;
		attr->attndims = attrs[i]->attndims;
		attr->attstorage = attrs[i]->attstorage;
		attr->attcompression = attrs[i]->attcompression;
		attr->attcollation = attrs[i]->attcollation;
		attr->attnotnull = attrs[i]->attnotnull;
		attr->atthasdef = attrs[i]->atthasdef;
		attr->atthasmissing = attrs[i]->atthasmissing;
		attr->attidentity = attrs[i]->attidentity;
		attr->attgenerated = attrs[i]->attgenerated;
		attr->attisdropped = attrs[i]->attisdropped;
		attr->attislocal = attrs[i]->attislocal;
		attr->attinhcount = attrs[i]->attinhcount;
		attr->attcollation = attrs[i]->attcollation;
	}

	return desc;
}

/*
 * CreateTupleDescCopy
 *		This function creates a new TupleDesc by copying from an existing
 *		TupleDesc.
 *
 * !!! Constraints and defaults are not copied !!!
 */
TupleDesc
CreateTupleDescCopy(TupleDesc tupdesc)
{
	TupleDesc	desc;
	int			i;

	desc = CreateTemplateTupleDesc(tupdesc->natts);

	/* Flat-copy the attribute array */
	memcpy(TupleDescAttr(desc, 0),
		   TupleDescAttr(tupdesc, 0),
		   desc->natts * sizeof(TupleDescAttr));

	/* Flat-copy the TupleDescAttrExtra fields */
	memcpy(TupleDescExtraAttr(desc->extra, 0),
		   TupleDescExtraAttr(tupdesc->extra, 0),
		   desc->natts * sizeof(TupleDescAttrExtra));

	/* We can copy the tuple type identification, too */
	desc->tdtypeid = tupdesc->tdtypeid;
	desc->tdtypmod = tupdesc->tdtypmod;

	return desc;
}

/*
 * CreateTupleDescCopyConstr
 *		This function creates a new TupleDesc by copying from an existing
 *		TupleDesc (including its constraints and defaults).
 */
TupleDesc
CreateTupleDescCopyConstr(TupleDesc tupdesc)
{
	TupleDesc	desc;
	TupleDescExtra *srcextra = tupdesc->extra;
	TupleDescExtra *dstextra;
	int			i;

	desc = CreateTemplateTupleDesc(tupdesc->natts);
	dstextra = desc->extra;

	/* Flat-copy the attribute array */
	memcpy(TupleDescAttr(desc, 0),
		   TupleDescAttr(tupdesc, 0),
		   desc->natts * sizeof(TupleDescAttr));

	/* Flat-copy the TupleDescAttrExtra fields */
	memcpy(TupleDescExtraAttr(dstextra, 0),
		   TupleDescExtraAttr(srcextra, 0),
		   desc->natts * sizeof(TupleDescAttrExtra));

	/* Copy the TupleDescExtra data structure */

	dstextra->has_not_null = srcextra->has_not_null;
	dstextra->has_generated_stored = srcextra->has_generated_stored;

	if ((dstextra->num_defval = srcextra->num_defval) > 0)
	{
		dstextra->defval = (AttrDefault *) palloc(dstextra->num_defval * sizeof(AttrDefault));
		memcpy(dstextra->defval, srcextra->defval, dstextra->num_defval * sizeof(AttrDefault));
		for (i = dstextra->num_defval - 1; i >= 0; i--)
			dstextra->defval[i].adbin = pstrdup(srcextra->defval[i].adbin);
	}

	if (srcextra->missing)
	{
		dstextra->missing = (AttrMissing *) palloc(tupdesc->natts * sizeof(AttrMissing));
		memcpy(dstextra->missing, srcextra->missing, tupdesc->natts * sizeof(AttrMissing));
		for (i = tupdesc->natts - 1; i >= 0; i--)
		{
			if (srcextra->missing[i].am_present)
			{
				TupleDescAttr *attr = TupleDescAttr(tupdesc, i);

				dstextra->missing[i].am_value = datumCopy(srcextra->missing[i].am_value,
														attr->attbyval,
														attr->attlen);
			}
		}
	}

	if ((dstextra->num_check = srcextra->num_check) > 0)
	{
		dstextra->check = (ConstrCheck *) palloc(dstextra->num_check * sizeof(ConstrCheck));
		memcpy(dstextra->check, srcextra->check, dstextra->num_check * sizeof(ConstrCheck));
		for (i = dstextra->num_check - 1; i >= 0; i--)
		{
			dstextra->check[i].ccname = pstrdup(srcextra->check[i].ccname);
			dstextra->check[i].ccbin = pstrdup(srcextra->check[i].ccbin);
			dstextra->check[i].ccvalid = srcextra->check[i].ccvalid;
			dstextra->check[i].ccnoinherit = srcextra->check[i].ccnoinherit;
		}
	}

	/* We can copy the tuple type identification, too */
	desc->tdtypeid = tupdesc->tdtypeid;
	desc->tdtypmod = tupdesc->tdtypmod;

	return desc;
}

/*
 * TupleDescCopy
 *		Copy a tuple descriptor into caller-supplied memory.
 *		The memory may be shared memory mapped at any address, and must
 *		be sufficient to hold TupleDescSize(src) bytes.
 *
 * !!! Constraints and defaults are not copied !!!
 */
void
TupleDescCopy(TupleDesc dst, TupleDesc src)
{
	int			i;

	/* Flat-copy the header and attribute array */
	memcpy(dst, src, TupleDescSize(src));

	/* Flat-copy the TupleDescAttrExtra fields */
	memcpy(TupleDescExtraAttr(dst->extra, 0),
		   TupleDescExtraAttr(src->extra, 0),
		   dst->natts * sizeof(TupleDescAttrExtra));

	/*
	 * Also, assume the destination is not to be ref-counted.  (Copying the
	 * source's refcount would be wrong in any case.)
	 */
	dst->tdrefcount = -1;
}

/*
 * TupleDescCopyEntry
 *		This function copies a single attribute structure from one tuple
 *		descriptor to another.
 *
 * !!! Constraints and defaults are not copied !!!
 */
void
TupleDescCopyEntry(TupleDesc dst, AttrNumber dstAttno,
				   TupleDesc src, AttrNumber srcAttno)
{
	TupleDescAttr *dstAtt = TupleDescAttr(dst, dstAttno - 1);
	TupleDescAttr *srcAtt = TupleDescAttr(src, srcAttno - 1);
	TupleDescAttrExtra *dstAttEx = TupleDescExtraAttr(dst->extra, dstAttno - 1);
	TupleDescAttrExtra *srcAttEx = TupleDescExtraAttr(src->extra, srcAttno - 1);


	/*
	 * sanity checks
	 */
	Assert(PointerIsValid(src));
	Assert(PointerIsValid(dst));
	Assert(srcAttno >= 1);
	Assert(srcAttno <= src->natts);
	Assert(dstAttno >= 1);
	Assert(dstAttno <= dst->natts);

	memcpy(dstAtt, srcAtt, sizeof(TupleDescAttr));
	memcpy(dstAttEx, srcAttEx, sizeof(TupleDescAttrExtra));

	/*
	 * Aside from updating the attno, we'd better reset attcacheoff.
	 *
	 * XXX Actually, to be entirely safe we'd need to reset the attcacheoff of
	 * all following columns in dst as well.  Current usage scenarios don't
	 * require that though, because all following columns will get initialized
	 * by other uses of this function or TupleDescInitEntry.  So we cheat a
	 * bit to avoid a useless O(N^2) penalty.
	 */
	dstAttEx->attnum = dstAttno;
	dstAtt->attcacheoff = -1;

	/* since we're not copying constraints or defaults, clear these */
	dstAttEx->attnotnull = false;
}

/*
 * Free a TupleDesc including all substructure
 */
void
FreeTupleDesc(TupleDesc tupdesc)
{
	int			i;

	/*
	 * Possibly this should assert tdrefcount == 0, to disallow explicit
	 * freeing of un-refcounted tupdescs?
	 */
	Assert(tupdesc->tdrefcount <= 0);

	if (tupdesc->extra->num_defval > 0)
	{
		AttrDefault *attrdef = tupdesc->extra->defval;

		for (i = tupdesc->extra->num_defval - 1; i >= 0; i--)
			pfree(attrdef[i].adbin);
		pfree(attrdef);
	}
	if (tupdesc->extra->missing)
	{
		AttrMissing *attrmiss = tupdesc->extra->missing;

		for (i = tupdesc->natts - 1; i >= 0; i--)
		{
			if (attrmiss[i].am_present
				&& !TupleDescAttr(tupdesc, i)->attbyval)
				pfree(DatumGetPointer(attrmiss[i].am_value));
		}
		pfree(attrmiss);
	}
	if (tupdesc->extra->num_check > 0)
	{
		ConstrCheck *check = tupdesc->extra->check;

		for (i = tupdesc->extra->num_check - 1; i >= 0; i--)
		{
			pfree(check[i].ccname);
			pfree(check[i].ccbin);
		}
		pfree(check);
	}

	pfree(tupdesc->extra);
	pfree(tupdesc);
}

/*
 * Increment the reference count of a tupdesc, and log the reference in
 * CurrentResourceOwner.
 *
 * Do not apply this to tupdescs that are not being refcounted.  (Use the
 * macro PinTupleDesc for tupdescs of uncertain status.)
 */
void
IncrTupleDescRefCount(TupleDesc tupdesc)
{
	Assert(tupdesc->tdrefcount >= 0);

	ResourceOwnerEnlarge(CurrentResourceOwner);
	tupdesc->tdrefcount++;
	ResourceOwnerRememberTupleDesc(CurrentResourceOwner, tupdesc);
}

/*
 * Decrement the reference count of a tupdesc, remove the corresponding
 * reference from CurrentResourceOwner, and free the tupdesc if no more
 * references remain.
 *
 * Do not apply this to tupdescs that are not being refcounted.  (Use the
 * macro ReleaseTupleDesc for tupdescs of uncertain status.)
 */
void
DecrTupleDescRefCount(TupleDesc tupdesc)
{
	Assert(tupdesc->tdrefcount > 0);

	ResourceOwnerForgetTupleDesc(CurrentResourceOwner, tupdesc);
	if (--tupdesc->tdrefcount == 0)
		FreeTupleDesc(tupdesc);
}

/*
 * Compare two TupleDesc structures for logical equality
 */
bool
equalTupleDescs(TupleDesc tupdesc1, TupleDesc tupdesc2)
{
	TupleDescExtra *extra1;
	TupleDescExtra *extra2;
	int			i,
				n;

	if (tupdesc1->natts != tupdesc2->natts)
		return false;
	if (tupdesc1->tdtypeid != tupdesc2->tdtypeid)
		return false;

	/* tdtypmod and tdrefcount are not checked */

	extra1 = tupdesc1->extra;
	extra2 = tupdesc2->extra;

	for (i = 0; i < tupdesc1->natts; i++)
	{
		TupleDescAttr *attr1 = TupleDescAttr(tupdesc1, i);
		TupleDescAttr *attr2 = TupleDescAttr(tupdesc2, i);
		TupleDescAttrExtra *attr1ex = TupleDescExtraAttr(extra1, i);
		TupleDescAttrExtra *attr2ex = TupleDescExtraAttr(extra2, i);


		/*
		 * We do not need to check every single field here: we can disregard
		 * attrelid and attnum (which were used to place the row in the attrs
		 * array in the first place).  It might look like we could dispense
		 * with checking attlen/attbyval/attalign, since these are derived
		 * from atttypid; but in the case of dropped columns we must check
		 * them (since atttypid will be zero for all dropped columns) and in
		 * general it seems safer to check them always.
		 *
		 * attcacheoff must NOT be checked since it's possibly not set in both
		 * copies.  We also intentionally ignore atthasmissing, since that's
		 * not very relevant in tupdescs, which lack the attmissingval field.
		 */
		if (strcmp(NameStr(attr1ex->attname), NameStr(attr2ex->attname)) != 0)
			return false;
		if (attr1ex->atttypid != attr2ex->atttypid)
			return false;
		if (attr1->attlen != attr2->attlen)
			return false;
		if (attr1ex->attndims != attr2ex->attndims)
			return false;
		if (attr1ex->atttypmod != attr2ex->atttypmod)
			return false;
		if (attr1->attbyval != attr2->attbyval)
			return false;
		if (attr1->attalign != attr2->attalign)
			return false;
		if (attr1ex->attstorage != attr2ex->attstorage)
			return false;
		if (attr1ex->attcompression != attr2ex->attcompression)
			return false;
		if (attr1ex->attnotnull != attr2ex->attnotnull)
			return false;
		if (attr1ex->atthasdef != attr2ex->atthasdef)
			return false;
		if (attr1ex->attidentity != attr2ex->attidentity)
			return false;
		if (attr1ex->attgenerated != attr2ex->attgenerated)
			return false;
		if (attr1ex->attisdropped != attr2ex->attisdropped)
			return false;
		if (attr1ex->attislocal != attr2ex->attislocal)
			return false;
		if (attr1ex->attinhcount != attr2ex->attinhcount)
			return false;
		if (attr1ex->attcollation != attr2ex->attcollation)
			return false;
	}

	if (extra1->has_not_null != extra2->has_not_null)
		return false;
	if (extra1->has_generated_stored != extra2->has_generated_stored)
		return false;
	n = extra1->num_defval;
	if (n != (int) extra2->num_defval)
		return false;
	/* We assume here that both AttrDefault arrays are in adnum order */
	for (i = 0; i < n; i++)
	{
		AttrDefault *defval1 = extra1->defval + i;
		AttrDefault *defval2 = extra2->defval + i;

		if (defval1->adnum != defval2->adnum)
			return false;
		if (strcmp(defval1->adbin, defval2->adbin) != 0)
			return false;
	}
	if (extra1->missing)
	{
		if (!extra2->missing)
			return false;
		for (i = 0; i < tupdesc1->natts; i++)
		{
			AttrMissing *missval1 = extra1->missing + i;
			AttrMissing *missval2 = extra2->missing + i;

			if (missval1->am_present != missval2->am_present)
				return false;
			if (missval1->am_present)
			{
				TupleDescAttr *missatt1 = TupleDescAttr(tupdesc1, i);

				if (!datumIsEqual(missval1->am_value, missval2->am_value,
									missatt1->attbyval, missatt1->attlen))
					return false;
			}
		}
	}
	else if (extra2->missing)
		return false;
	n = extra1->num_check;
	if (n != (int) extra2->num_check)
		return false;

	/*
		* Similarly, we rely here on the ConstrCheck entries being sorted by
		* name.  If there are duplicate names, the outcome of the comparison
		* is uncertain, but that should not happen.
		*/
	for (i = 0; i < n; i++)
	{
		ConstrCheck *check1 = extra1->check + i;
		ConstrCheck *check2 = extra2->check + i;

		if (!(strcmp(check1->ccname, check2->ccname) == 0 &&
				strcmp(check1->ccbin, check2->ccbin) == 0 &&
				check1->ccvalid == check2->ccvalid &&
				check1->ccnoinherit == check2->ccnoinherit))
			return false;
	}

	return true;
}

/*
 * equalRowTypes
 *
 * This determines whether two tuple descriptors have equal row types.  This
 * only checks those fields in pg_attribute that are applicable for row types,
 * while ignoring those fields that define the physical row storage or those
 * that define table column metadata.
 *
 * Specifically, this checks:
 *
 * - same number of attributes
 * - same composite type ID (but could both be zero)
 * - corresponding attributes (in order) have same the name, type, typmod,
 *   collation
 *
 * This is used to check whether two record types are compatible, whether
 * function return row types are the same, and other similar situations.
 *
 * (XXX There was some discussion whether attndims should be checked here, but
 * for now it has been decided not to.)
 *
 * Note: We deliberately do not check the tdtypmod field.  This allows
 * typcache.c to use this routine to see if a cached record type matches a
 * requested type.
 */
bool
equalRowTypes(TupleDesc tupdesc1, TupleDesc tupdesc2)
{
	TupleDescExtra *extra1;
	TupleDescExtra *extra2;

	if (tupdesc1->natts != tupdesc2->natts)
		return false;
	if (tupdesc1->tdtypeid != tupdesc2->tdtypeid)
		return false;

	extra1 = tupdesc1->extra;
	extra2 = tupdesc2->extra;

	for (int i = 0; i < tupdesc1->natts; i++)
	{
		TupleDescAttr *attr1 = TupleDescAttr(tupdesc1, i);
		TupleDescAttr *attr2 = TupleDescAttr(tupdesc2, i);
		TupleDescAttrExtra *attr1ex = TupleDescExtraAttr(extra1, i);
		TupleDescAttrExtra *attr2ex = TupleDescExtraAttr(extra2, i);

		if (strcmp(NameStr(attr1ex->attname), NameStr(attr2ex->attname)) != 0)
			return false;
		if (attr1ex->atttypid != attr2ex->atttypid)
			return false;
		if (attr1ex->atttypmod != attr2ex->atttypmod)
			return false;
		if (attr1ex->attcollation != attr2ex->attcollation)
			return false;

		/* Record types derived from tables could have dropped fields. */
		if (attr1ex->attisdropped != attr2ex->attisdropped)
			return false;
	}

	return true;
}

/*
 * hashRowType
 *
 * If two tuple descriptors would be considered equal by equalRowTypes()
 * then their hash value will be equal according to this function.
 */
uint32
hashRowType(TupleDesc desc)
{
	TupleDescExtra *extra = desc->extra;
	uint32		s;
	int			i;

	s = hash_combine(0, hash_uint32(desc->natts));
	s = hash_combine(s, hash_uint32(desc->tdtypeid));
	for (i = 0; i < desc->natts; ++i)
		s = hash_combine(s, hash_uint32(TupleDescExtraAttr(extra, i)->atttypid));

	return s;
}

/*
 * TupleDescInitEntry
 *		This function initializes a single attribute structure in
 *		a previously allocated tuple descriptor.
 *
 * If attributeName is NULL, the attname field is set to an empty string
 * (this is for cases where we don't know or need a name for the field).
 * Also, some callers use this function to change the datatype-related fields
 * in an existing tupdesc; they pass attributeName = NameStr(att->attname)
 * to indicate that the attname field shouldn't be modified.
 *
 * Note that attcollation is set to the default for the specified datatype.
 * If a nondefault collation is needed, insert it afterwards using
 * TupleDescInitEntryCollation.
 */
void
TupleDescInitEntry(TupleDesc desc,
				   AttrNumber attributeNumber,
				   const char *attributeName,
				   Oid oidtypeid,
				   int32 typmod,
				   int attdim)
{
	HeapTuple	tuple;
	Form_pg_type typeForm;
	TupleDescAttr *att;
	TupleDescAttrExtra *attEx;

	/*
	 * sanity checks
	 */
	Assert(PointerIsValid(desc));
	Assert(attributeNumber >= 1);
	Assert(attributeNumber <= desc->natts);
	Assert(attdim >= 0);
	Assert(attdim <= PG_INT16_MAX);

	/*
	 * initialize the attribute fields
	 */
	att = TupleDescAttr(desc, attributeNumber - 1);
	attEx = TupleDescExtraAttr(desc->extra, attributeNumber - 1);

	attEx->attrelid = 0;			/* dummy value */

	/*
	 * Note: attributeName can be NULL, because the planner doesn't always
	 * fill in valid resname values in targetlists, particularly for resjunk
	 * attributes. Also, do nothing if caller wants to re-use the old attname.
	 */
	if (attributeName == NULL)
		MemSet(NameStr(attEx->attname), 0, NAMEDATALEN);
	else if (attributeName != NameStr(attEx->attname))
		namestrcpy(&(attEx->attname), attributeName);

	att->attcacheoff = -1;
	attEx->atttypmod = typmod;

	attEx->attnum = attributeNumber;
	attEx->attndims = attdim;

	attEx->attnotnull = false;
	attEx->atthasdef = false;
	attEx->atthasmissing = false;
	attEx->attidentity = '\0';
	attEx->attgenerated = '\0';
	attEx->attisdropped = false;
	attEx->attislocal = true;
	attEx->attinhcount = 0;
	/* variable-length fields are not present in tupledescs */

	tuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(oidtypeid));
	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "cache lookup failed for type %u", oidtypeid);
	typeForm = (Form_pg_type) GETSTRUCT(tuple);

	attEx->atttypid = oidtypeid;
	att->attlen = typeForm->typlen;
	att->attbyval = typeForm->typbyval;
	att->attalign = typeForm->typalign;
	attEx->attstorage = typeForm->typstorage;
	attEx->attcompression = InvalidCompressionMethod;
	attEx->attcollation = typeForm->typcollation;

	ReleaseSysCache(tuple);
}

/*
 * TupleDescInitBuiltinEntry
 *		Initialize a tuple descriptor without catalog access.  Only
 *		a limited range of builtin types are supported.
 */
void
TupleDescInitBuiltinEntry(TupleDesc desc,
						  AttrNumber attributeNumber,
						  const char *attributeName,
						  Oid oidtypeid,
						  int32 typmod,
						  int attdim)
{
	TupleDescAttr *att;
	TupleDescAttrExtra *attEx;

	/* sanity checks */
	Assert(PointerIsValid(desc));
	Assert(attributeNumber >= 1);
	Assert(attributeNumber <= desc->natts);
	Assert(attdim >= 0);
	Assert(attdim <= PG_INT16_MAX);

	/* initialize the attribute fields */
	att = TupleDescAttr(desc, attributeNumber - 1);
	attEx = TupleDescExtraAttr(desc->extra, attributeNumber - 1);

	attEx->attrelid = 0;			/* dummy value */

	/* unlike TupleDescInitEntry, we require an attribute name */
	Assert(attributeName != NULL);
	namestrcpy(&(attEx->attname), attributeName);

	att->attcacheoff = -1;
	attEx->atttypmod = typmod;

	attEx->attnum = attributeNumber;
	attEx->attndims = attdim;

	attEx->attnotnull = false;
	attEx->atthasdef = false;
	attEx->atthasmissing = false;
	attEx->attidentity = '\0';
	attEx->attgenerated = '\0';
	attEx->attisdropped = false;
	attEx->attislocal = true;
	attEx->attinhcount = 0;

	attEx->atttypid = oidtypeid;

	/*
	 * Our goal here is to support just enough types to let basic builtin
	 * commands work without catalog access - e.g. so that we can do certain
	 * things even in processes that are not connected to a database.
	 */
	switch (oidtypeid)
	{
		case TEXTOID:
		case TEXTARRAYOID:
			att->attlen = -1;
			att->attbyval = false;
			att->attalign = TYPALIGN_INT;
			attEx->attstorage = TYPSTORAGE_EXTENDED;
			attEx->attcompression = InvalidCompressionMethod;
			attEx->attcollation = DEFAULT_COLLATION_OID;
			break;

		case BOOLOID:
			att->attlen = 1;
			att->attbyval = true;
			att->attalign = TYPALIGN_CHAR;
			attEx->attstorage = TYPSTORAGE_PLAIN;
			attEx->attcompression = InvalidCompressionMethod;
			attEx->attcollation = InvalidOid;
			break;

		case INT4OID:
			att->attlen = 4;
			att->attbyval = true;
			att->attalign = TYPALIGN_INT;
			attEx->attstorage = TYPSTORAGE_PLAIN;
			attEx->attcompression = InvalidCompressionMethod;
			attEx->attcollation = InvalidOid;
			break;

		case INT8OID:
			att->attlen = 8;
			att->attbyval = FLOAT8PASSBYVAL;
			att->attalign = TYPALIGN_DOUBLE;
			attEx->attstorage = TYPSTORAGE_PLAIN;
			attEx->attcompression = InvalidCompressionMethod;
			attEx->attcollation = InvalidOid;
			break;

		case OIDOID:
			att->attlen = 4;
			att->attbyval = true;
			att->attalign = TYPALIGN_INT;
			attEx->attstorage = TYPSTORAGE_PLAIN;
			attEx->attcompression = InvalidCompressionMethod;
			attEx->attcollation = InvalidOid;
			break;

		default:
			elog(ERROR, "unsupported type %u", oidtypeid);
	}
}

/*
 * TupleDescInitEntryCollation
 *
 * Assign a nondefault collation to a previously initialized tuple descriptor
 * entry.
 */
void
TupleDescInitEntryCollation(TupleDesc desc,
							AttrNumber attributeNumber,
							Oid collationid)
{
	/*
	 * sanity checks
	 */
	Assert(PointerIsValid(desc));
	Assert(attributeNumber >= 1);
	Assert(attributeNumber <= desc->natts);

	TupleDescExtraAttr(desc->extra, attributeNumber - 1)->attcollation = collationid;
}

/*
 * BuildDescFromLists
 *
 * Build a TupleDesc given lists of column names (as String nodes),
 * column type OIDs, typmods, and collation OIDs.
 *
 * No constraints are generated.
 *
 * This is for use with functions returning RECORD.
 */
TupleDesc
BuildDescFromLists(const List *names, const List *types, const List *typmods, const List *collations)
{
	int			natts;
	AttrNumber	attnum;
	ListCell   *l1;
	ListCell   *l2;
	ListCell   *l3;
	ListCell   *l4;
	TupleDesc	desc;

	natts = list_length(names);
	Assert(natts == list_length(types));
	Assert(natts == list_length(typmods));
	Assert(natts == list_length(collations));

	/*
	 * allocate a new tuple descriptor
	 */
	desc = CreateTemplateTupleDesc(natts);

	attnum = 0;
	forfour(l1, names, l2, types, l3, typmods, l4, collations)
	{
		char	   *attname = strVal(lfirst(l1));
		Oid			atttypid = lfirst_oid(l2);
		int32		atttypmod = lfirst_int(l3);
		Oid			attcollation = lfirst_oid(l4);

		attnum++;

		TupleDescInitEntry(desc, attnum, attname, atttypid, atttypmod, 0);
		TupleDescInitEntryCollation(desc, attnum, attcollation);
	}

	return desc;
}

/*
 * Get default expression (or NULL if none) for the given attribute number.
 */
Node *
TupleDescGetDefault(TupleDesc tupdesc, AttrNumber attnum)
{
	Node	   *result = NULL;
	AttrDefault *attrdef = tupdesc->extra->defval;

	for (int i = 0; i < tupdesc->extra->num_defval; i++)
	{
		if (attrdef[i].adnum == attnum)
		{
			result = stringToNode(attrdef[i].adbin);
			break;
		}
	}

	return result;
}

/* ResourceOwner callbacks */

static void
ResOwnerReleaseTupleDesc(Datum res)
{
	TupleDesc	tupdesc = (TupleDesc) DatumGetPointer(res);

	/* Like DecrTupleDescRefCount, but don't call ResourceOwnerForget() */
	Assert(tupdesc->tdrefcount > 0);
	if (--tupdesc->tdrefcount == 0)
		FreeTupleDesc(tupdesc);
}

static char *
ResOwnerPrintTupleDesc(Datum res)
{
	TupleDesc	tupdesc = (TupleDesc) DatumGetPointer(res);

	return psprintf("TupleDesc %p (%u,%d)",
					tupdesc, tupdesc->tdtypeid, tupdesc->tdtypmod);
}
