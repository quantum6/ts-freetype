/****************************************************************************
 *
 * gxvmort2.c
 *
 *   TrueTypeGX/AAT mort table validation
 *   body for type2 (Ligature Substitution) subtable.
 *
 * Copyright (C) 2005-2022 by
 * suzuki toshiya, Masatake YAMATO, Red Hat K.K.,
 * David Turner, Robert Wilhelm, and Werner Lemberg.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.  By continuing to use, modify, or distribute
 * this file you indicate that you have read the license and
 * understand and accept it fully.
 *
 */

/****************************************************************************
 *
 * gxvalid is derived from both gxlayout module and otvalid module.
 * Development of gxlayout is supported by the Information-technology
 * Promotion Agency(IPA), Japan.
 *
 */


#include "gxvmort.h"


  /**************************************************************************
   *
   * The macro FT_TS_COMPONENT is used in trace mode.  It is an implicit
   * parameter of the FT_TS_TRACE() and FT_TS_ERROR() macros, used to print/log
   * messages during execution.
   */
#undef  FT_TS_COMPONENT
#define FT_TS_COMPONENT  gxvmort


  typedef struct  GXV_mort_subtable_type2_StateOptRec_
  {
    FT_TS_UShort  ligActionTable;
    FT_TS_UShort  componentTable;
    FT_TS_UShort  ligatureTable;
    FT_TS_UShort  ligActionTable_length;
    FT_TS_UShort  componentTable_length;
    FT_TS_UShort  ligatureTable_length;

  }  GXV_mort_subtable_type2_StateOptRec,
    *GXV_mort_subtable_type2_StateOptRecData;

#define GXV_MORT_SUBTABLE_TYPE2_HEADER_SIZE \
          ( GXV_STATETABLE_HEADER_SIZE + 2 + 2 + 2 )


  static void
  gxv_mort_subtable_type2_opttable_load( FT_TS_Bytes       table,
                                         FT_TS_Bytes       limit,
                                         GXV_Validator  gxvalid )
  {
    FT_TS_Bytes  p = table;
    GXV_mort_subtable_type2_StateOptRecData  optdata =
      (GXV_mort_subtable_type2_StateOptRecData)gxvalid->statetable.optdata;


    GXV_LIMIT_CHECK( 2 + 2 + 2 );
    optdata->ligActionTable = FT_TS_NEXT_USHORT( p );
    optdata->componentTable = FT_TS_NEXT_USHORT( p );
    optdata->ligatureTable  = FT_TS_NEXT_USHORT( p );

    GXV_TRACE(( "offset to ligActionTable=0x%04x\n",
                optdata->ligActionTable ));
    GXV_TRACE(( "offset to componentTable=0x%04x\n",
                optdata->componentTable ));
    GXV_TRACE(( "offset to ligatureTable=0x%04x\n",
                optdata->ligatureTable ));
  }


  static void
  gxv_mort_subtable_type2_subtable_setup( FT_TS_UShort      table_size,
                                          FT_TS_UShort      classTable,
                                          FT_TS_UShort      stateArray,
                                          FT_TS_UShort      entryTable,
                                          FT_TS_UShort      *classTable_length_p,
                                          FT_TS_UShort      *stateArray_length_p,
                                          FT_TS_UShort      *entryTable_length_p,
                                          GXV_Validator  gxvalid )
  {
    FT_TS_UShort  o[6];
    FT_TS_UShort  *l[6];
    FT_TS_UShort  buff[7];

    GXV_mort_subtable_type2_StateOptRecData  optdata =
      (GXV_mort_subtable_type2_StateOptRecData)gxvalid->statetable.optdata;


    GXV_NAME_ENTER( "subtable boundaries setup" );

    o[0] = classTable;
    o[1] = stateArray;
    o[2] = entryTable;
    o[3] = optdata->ligActionTable;
    o[4] = optdata->componentTable;
    o[5] = optdata->ligatureTable;
    l[0] = classTable_length_p;
    l[1] = stateArray_length_p;
    l[2] = entryTable_length_p;
    l[3] = &(optdata->ligActionTable_length);
    l[4] = &(optdata->componentTable_length);
    l[5] = &(optdata->ligatureTable_length);

    gxv_set_length_by_ushort_offset( o, l, buff, 6, table_size, gxvalid );

    GXV_TRACE(( "classTable: offset=0x%04x length=0x%04x\n",
                classTable, *classTable_length_p ));
    GXV_TRACE(( "stateArray: offset=0x%04x length=0x%04x\n",
                stateArray, *stateArray_length_p ));
    GXV_TRACE(( "entryTable: offset=0x%04x length=0x%04x\n",
                entryTable, *entryTable_length_p ));
    GXV_TRACE(( "ligActionTable: offset=0x%04x length=0x%04x\n",
                optdata->ligActionTable,
                optdata->ligActionTable_length ));
    GXV_TRACE(( "componentTable: offset=0x%04x length=0x%04x\n",
                optdata->componentTable,
                optdata->componentTable_length ));
    GXV_TRACE(( "ligatureTable:  offset=0x%04x length=0x%04x\n",
                optdata->ligatureTable,
                optdata->ligatureTable_length ));

    GXV_EXIT;
  }


  static void
  gxv_mort_subtable_type2_ligActionOffset_validate(
    FT_TS_Bytes       table,
    FT_TS_UShort      ligActionOffset,
    GXV_Validator  gxvalid )
  {
    /* access ligActionTable */
    GXV_mort_subtable_type2_StateOptRecData  optdata =
      (GXV_mort_subtable_type2_StateOptRecData)gxvalid->statetable.optdata;

    FT_TS_Bytes lat_base  = table + optdata->ligActionTable;
    FT_TS_Bytes p         = table + ligActionOffset;
    FT_TS_Bytes lat_limit = lat_base + optdata->ligActionTable;


    GXV_32BIT_ALIGNMENT_VALIDATE( ligActionOffset );
    if ( p < lat_base )
    {
      GXV_TRACE(( "too short offset 0x%04x: p < lat_base (%ld byte rewind)\n",
                  ligActionOffset, lat_base - p ));

      /* FontValidator, ftxvalidator, ftxdumperfuser warn but continue */
      GXV_SET_ERR_IF_PARANOID( FT_TS_INVALID_OFFSET );
    }
    else if ( lat_limit < p )
    {
      GXV_TRACE(( "too large offset 0x%04x: lat_limit < p (%ld byte overrun)\n",
                  ligActionOffset, p - lat_limit ));

      /* FontValidator, ftxvalidator, ftxdumperfuser warn but continue */
      GXV_SET_ERR_IF_PARANOID( FT_TS_INVALID_OFFSET );
    }
    else
    {
      /* validate entry in ligActionTable */
      FT_TS_ULong   lig_action;
#ifdef GXV_LOAD_UNUSED_VARS
      FT_TS_UShort  last;
      FT_TS_UShort  store;
#endif
      FT_TS_ULong   offset;


      lig_action = FT_TS_NEXT_ULONG( p );
#ifdef GXV_LOAD_UNUSED_VARS
      last   = (FT_TS_UShort)( ( lig_action >> 31 ) & 1 );
      store  = (FT_TS_UShort)( ( lig_action >> 30 ) & 1 );
#endif

      /* Apple spec defines this offset as a word offset */
      offset = lig_action & 0x3FFFFFFFUL;
      if ( offset * 2 < optdata->ligatureTable )
      {
        GXV_TRACE(( "too short offset 0x%08lx:"
                    " 2 x offset < ligatureTable (%lu byte rewind)\n",
                     offset, optdata->ligatureTable - offset * 2 ));

        GXV_SET_ERR_IF_PARANOID( FT_TS_INVALID_OFFSET );
      } else if ( offset * 2 >
                  optdata->ligatureTable + optdata->ligatureTable_length )
      {
        GXV_TRACE(( "too long offset 0x%08lx:"
                    " 2 x offset > ligatureTable + ligatureTable_length"
                    " (%lu byte overrun)\n",
                     offset,
                     optdata->ligatureTable + optdata->ligatureTable_length
                     - offset * 2 ));

        GXV_SET_ERR_IF_PARANOID( FT_TS_INVALID_OFFSET );
      }
    }
  }


  static void
  gxv_mort_subtable_type2_entry_validate(
    FT_TS_Byte                         state,
    FT_TS_UShort                       flags,
    GXV_StateTable_GlyphOffsetCPtr  glyphOffset_p,
    FT_TS_Bytes                        table,
    FT_TS_Bytes                        limit,
    GXV_Validator                   gxvalid )
  {
#ifdef GXV_LOAD_UNUSED_VARS
    FT_TS_UShort setComponent;
    FT_TS_UShort dontAdvance;
#endif
    FT_TS_UShort offset;

    FT_TS_UNUSED( state );
    FT_TS_UNUSED( glyphOffset_p );
    FT_TS_UNUSED( limit );


#ifdef GXV_LOAD_UNUSED_VARS
    setComponent = (FT_TS_UShort)( ( flags >> 15 ) & 1 );
    dontAdvance  = (FT_TS_UShort)( ( flags >> 14 ) & 1 );
#endif

    offset = (FT_TS_UShort)( flags & 0x3FFFU );

    if ( 0 < offset )
      gxv_mort_subtable_type2_ligActionOffset_validate( table, offset,
                                                        gxvalid );
  }


  static void
  gxv_mort_subtable_type2_ligatureTable_validate( FT_TS_Bytes       table,
                                                  GXV_Validator  gxvalid )
  {
    GXV_mort_subtable_type2_StateOptRecData  optdata =
      (GXV_mort_subtable_type2_StateOptRecData)gxvalid->statetable.optdata;

    FT_TS_Bytes p     = table + optdata->ligatureTable;
    FT_TS_Bytes limit = table + optdata->ligatureTable
                           + optdata->ligatureTable_length;


    GXV_NAME_ENTER( "mort chain subtable type2 - substitutionTable" );
    if ( 0 != optdata->ligatureTable )
    {
      /* Apple does not give specification of ligatureTable format */
      while ( p < limit )
      {
        FT_TS_UShort  lig_gid;


        GXV_LIMIT_CHECK( 2 );
        lig_gid = FT_TS_NEXT_USHORT( p );

        if ( gxvalid->face->num_glyphs < lig_gid )
          GXV_SET_ERR_IF_PARANOID( FT_TS_INVALID_GLYPH_ID );
      }
    }
    GXV_EXIT;
  }


  FT_TS_LOCAL_DEF( void )
  gxv_mort_subtable_type2_validate( FT_TS_Bytes       table,
                                    FT_TS_Bytes       limit,
                                    GXV_Validator  gxvalid )
  {
    FT_TS_Bytes  p = table;

    GXV_mort_subtable_type2_StateOptRec  lig_rec;


    GXV_NAME_ENTER( "mort chain subtable type2 (Ligature Substitution)" );

    GXV_LIMIT_CHECK( GXV_MORT_SUBTABLE_TYPE2_HEADER_SIZE );

    gxvalid->statetable.optdata =
      &lig_rec;
    gxvalid->statetable.optdata_load_func =
      gxv_mort_subtable_type2_opttable_load;
    gxvalid->statetable.subtable_setup_func =
      gxv_mort_subtable_type2_subtable_setup;
    gxvalid->statetable.entry_glyphoffset_fmt =
      GXV_GLYPHOFFSET_NONE;
    gxvalid->statetable.entry_validate_func =
      gxv_mort_subtable_type2_entry_validate;

    gxv_StateTable_validate( p, limit, gxvalid );

    p += gxvalid->subtable_length;
    gxv_mort_subtable_type2_ligatureTable_validate( table, gxvalid );

    gxvalid->subtable_length = (FT_TS_ULong)( p - table );

    GXV_EXIT;
  }


/* END */
