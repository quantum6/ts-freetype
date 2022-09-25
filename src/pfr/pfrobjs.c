/****************************************************************************
 *
 * pfrobjs.c
 *
 *   FreeType PFR object methods (body).
 *
 * Copyright (C) 2002-2022 by
 * David Turner, Robert Wilhelm, and Werner Lemberg.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.  By continuing to use, modify, or distribute
 * this file you indicate that you have read the license and
 * understand and accept it fully.
 *
 */


#include "pfrobjs.h"
#include "pfrload.h"
#include "pfrgload.h"
#include "pfrcmap.h"
#include "pfrsbit.h"
#include <freetype/ftoutln.h>
#include <freetype/internal/ftdebug.h>
#include <freetype/internal/ftcalc.h>
#include <freetype/ttnameid.h>

#include "pfrerror.h"

#undef  FT_TS_COMPONENT
#define FT_TS_COMPONENT  pfr


  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****                     FACE OBJECT METHODS                       *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/

  FT_TS_LOCAL_DEF( void )
  pfr_face_done( FT_TS_Face  pfrface )     /* PFR_Face */
  {
    PFR_Face   face = (PFR_Face)pfrface;
    FT_TS_Memory  memory;


    if ( !face )
      return;

    memory = pfrface->driver->root.memory;

    /* we don't want dangling pointers */
    pfrface->family_name = NULL;
    pfrface->style_name  = NULL;

    /* finalize the physical font record */
    pfr_phy_font_done( &face->phy_font, FT_TS_FACE_MEMORY( face ) );

    /* no need to finalize the logical font or the header */
    FT_TS_FREE( pfrface->available_sizes );
  }


  FT_TS_LOCAL_DEF( FT_TS_Error )
  pfr_face_init( FT_TS_Stream      stream,
                 FT_TS_Face        pfrface,
                 FT_TS_Int         face_index,
                 FT_TS_Int         num_params,
                 FT_TS_Parameter*  params )
  {
    PFR_Face  face = (PFR_Face)pfrface;
    FT_TS_Error  error;

    FT_TS_UNUSED( num_params );
    FT_TS_UNUSED( params );


    FT_TS_TRACE2(( "PFR driver\n" ));

    /* load the header and check it */
    error = pfr_header_load( &face->header, stream );
    if ( error )
    {
      FT_TS_TRACE2(( "  not a PFR font\n" ));
      error = FT_TS_THROW( Unknown_File_Format );
      goto Exit;
    }

    if ( !pfr_header_check( &face->header ) )
    {
      FT_TS_TRACE2(( "  not a PFR font\n" ));
      error = FT_TS_THROW( Unknown_File_Format );
      goto Exit;
    }

    /* check face index */
    {
      FT_TS_Long  num_faces;


      error = pfr_log_font_count( stream,
                                  face->header.log_dir_offset,
                                  &num_faces );
      if ( error )
        goto Exit;

      pfrface->num_faces = num_faces;
    }

    if ( face_index < 0 )
      goto Exit;

    if ( ( face_index & 0xFFFF ) >= pfrface->num_faces )
    {
      FT_TS_ERROR(( "pfr_face_init: invalid face index\n" ));
      error = FT_TS_THROW( Invalid_Argument );
      goto Exit;
    }

    /* load the face */
    error = pfr_log_font_load(
              &face->log_font,
              stream,
              (FT_TS_UInt)( face_index & 0xFFFF ),
              face->header.log_dir_offset,
              FT_TS_BOOL( face->header.phy_font_max_size_high ) );
    if ( error )
      goto Exit;

    /* now load the physical font descriptor */
    error = pfr_phy_font_load( &face->phy_font, stream,
                               face->log_font.phys_offset,
                               face->log_font.phys_size );
    if ( error )
      goto Exit;

    /* now set up all root face fields */
    {
      PFR_PhyFont  phy_font = &face->phy_font;


      pfrface->face_index = face_index & 0xFFFF;
      pfrface->num_glyphs = (FT_TS_Long)phy_font->num_chars + 1;

      pfrface->face_flags |= FT_TS_FACE_FLAG_SCALABLE;

      /* if gps_offset == 0 for all characters, we  */
      /* assume that the font only contains bitmaps */
      {
        FT_TS_UInt  nn;


        for ( nn = 0; nn < phy_font->num_chars; nn++ )
          if ( phy_font->chars[nn].gps_offset != 0 )
            break;

        if ( nn == phy_font->num_chars )
        {
          if ( phy_font->num_strikes > 0 )
            pfrface->face_flags = 0;        /* not scalable */
          else
          {
            FT_TS_ERROR(( "pfr_face_init: font doesn't contain glyphs\n" ));
            error = FT_TS_THROW( Invalid_File_Format );
            goto Exit;
          }
        }
      }

      if ( ( phy_font->flags & PFR_PHY_PROPORTIONAL ) == 0 )
        pfrface->face_flags |= FT_TS_FACE_FLAG_FIXED_WIDTH;

      if ( phy_font->flags & PFR_PHY_VERTICAL )
        pfrface->face_flags |= FT_TS_FACE_FLAG_VERTICAL;
      else
        pfrface->face_flags |= FT_TS_FACE_FLAG_HORIZONTAL;

      if ( phy_font->num_strikes > 0 )
        pfrface->face_flags |= FT_TS_FACE_FLAG_FIXED_SIZES;

      if ( phy_font->num_kern_pairs > 0 )
        pfrface->face_flags |= FT_TS_FACE_FLAG_KERNING;

      /* If no family name was found in the `undocumented' auxiliary
       * data, use the font ID instead.  This sucks but is better than
       * nothing.
       */
      pfrface->family_name = phy_font->family_name;
      if ( !pfrface->family_name )
        pfrface->family_name = phy_font->font_id;

      /* note that the style name can be NULL in certain PFR fonts,
       * probably meaning `Regular'
       */
      pfrface->style_name = phy_font->style_name;

      pfrface->num_fixed_sizes = 0;
      pfrface->available_sizes = NULL;

      pfrface->bbox         = phy_font->bbox;
      pfrface->units_per_EM = (FT_TS_UShort)phy_font->outline_resolution;
      pfrface->ascender     = (FT_TS_Short) phy_font->bbox.yMax;
      pfrface->descender    = (FT_TS_Short) phy_font->bbox.yMin;

      pfrface->height = (FT_TS_Short)( ( pfrface->units_per_EM * 12 ) / 10 );
      if ( pfrface->height < pfrface->ascender - pfrface->descender )
        pfrface->height = (FT_TS_Short)( pfrface->ascender - pfrface->descender );

      if ( phy_font->num_strikes > 0 )
      {
        FT_TS_UInt          n, count = phy_font->num_strikes;
        FT_TS_Bitmap_Size*  size;
        PFR_Strike       strike;
        FT_TS_Memory        memory = pfrface->stream->memory;


        if ( FT_TS_QNEW_ARRAY( pfrface->available_sizes, count ) )
          goto Exit;

        size   = pfrface->available_sizes;
        strike = phy_font->strikes;
        for ( n = 0; n < count; n++, size++, strike++ )
        {
          size->height = (FT_TS_Short)strike->y_ppm;
          size->width  = (FT_TS_Short)strike->x_ppm;
          size->size   = (FT_TS_Pos)( strike->y_ppm << 6 );
          size->x_ppem = (FT_TS_Pos)( strike->x_ppm << 6 );
          size->y_ppem = (FT_TS_Pos)( strike->y_ppm << 6 );
        }
        pfrface->num_fixed_sizes = (FT_TS_Int)count;
      }

      /* now compute maximum advance width */
      if ( ( phy_font->flags & PFR_PHY_PROPORTIONAL ) == 0 )
        pfrface->max_advance_width = (FT_TS_Short)phy_font->standard_advance;
      else
      {
        FT_TS_Int    max = 0;
        FT_TS_UInt   count = phy_font->num_chars;
        PFR_Char  gchar = phy_font->chars;


        for ( ; count > 0; count--, gchar++ )
        {
          if ( max < gchar->advance )
            max = gchar->advance;
        }

        pfrface->max_advance_width = (FT_TS_Short)max;
      }

      pfrface->max_advance_height = pfrface->height;

      pfrface->underline_position  = (FT_TS_Short)( -pfrface->units_per_EM / 10 );
      pfrface->underline_thickness = (FT_TS_Short)(  pfrface->units_per_EM / 30 );

      /* create charmap */
      {
        FT_TS_CharMapRec  charmap;


        charmap.face        = pfrface;
        charmap.platform_id = TT_PLATFORM_MICROSOFT;
        charmap.encoding_id = TT_MS_ID_UNICODE_CS;
        charmap.encoding    = FT_TS_ENCODING_UNICODE;

        error = FT_TS_CMap_New( &pfr_cmap_class_rec, NULL, &charmap, NULL );
      }

      /* check whether we have loaded any kerning pairs */
      if ( phy_font->num_kern_pairs )
        pfrface->face_flags |= FT_TS_FACE_FLAG_KERNING;
    }

  Exit:
    return error;
  }


  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****                    SLOT OBJECT METHOD                         *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/

  FT_TS_LOCAL_DEF( FT_TS_Error )
  pfr_slot_init( FT_TS_GlyphSlot  pfrslot )        /* PFR_Slot */
  {
    PFR_Slot        slot   = (PFR_Slot)pfrslot;
    FT_TS_GlyphLoader  loader = pfrslot->internal->loader;


    pfr_glyph_init( &slot->glyph, loader );

    return 0;
  }


  FT_TS_LOCAL_DEF( void )
  pfr_slot_done( FT_TS_GlyphSlot  pfrslot )        /* PFR_Slot */
  {
    PFR_Slot  slot = (PFR_Slot)pfrslot;


    pfr_glyph_done( &slot->glyph );
  }


  FT_TS_LOCAL_DEF( FT_TS_Error )
  pfr_slot_load( FT_TS_GlyphSlot  pfrslot,         /* PFR_Slot */
                 FT_TS_Size       pfrsize,         /* PFR_Size */
                 FT_TS_UInt       gindex,
                 FT_TS_Int32      load_flags )
  {
    PFR_Slot     slot    = (PFR_Slot)pfrslot;
    PFR_Size     size    = (PFR_Size)pfrsize;
    FT_TS_Error     error;
    PFR_Face     face    = (PFR_Face)pfrslot->face;
    PFR_Char     gchar;
    FT_TS_Outline*  outline = &pfrslot->outline;
    FT_TS_ULong     gps_offset;


    FT_TS_TRACE1(( "pfr_slot_load: glyph index %d\n", gindex ));

    if ( gindex > 0 )
      gindex--;

    if ( !face || gindex >= face->phy_font.num_chars )
    {
      error = FT_TS_THROW( Invalid_Argument );
      goto Exit;
    }

    /* try to load an embedded bitmap */
    if ( ( load_flags & ( FT_TS_LOAD_NO_SCALE | FT_TS_LOAD_NO_BITMAP ) ) == 0 )
    {
      error = pfr_slot_load_bitmap(
                slot,
                size,
                gindex,
                ( load_flags & FT_TS_LOAD_BITMAP_METRICS_ONLY ) != 0 );
      if ( !error )
        goto Exit;
    }

    if ( load_flags & FT_TS_LOAD_SBITS_ONLY )
    {
      error = FT_TS_THROW( Invalid_Argument );
      goto Exit;
    }

    gchar               = face->phy_font.chars + gindex;
    pfrslot->format     = FT_TS_GLYPH_FORMAT_OUTLINE;
    outline->n_points   = 0;
    outline->n_contours = 0;
    gps_offset          = face->header.gps_section_offset;

    /* load the glyph outline (FT_TS_LOAD_NO_RECURSE isn't supported) */
    error = pfr_glyph_load( &slot->glyph, face->root.stream,
                            gps_offset, gchar->gps_offset, gchar->gps_size );

    if ( !error )
    {
      FT_TS_BBox            cbox;
      FT_TS_Glyph_Metrics*  metrics = &pfrslot->metrics;
      FT_TS_Pos             advance;
      FT_TS_UInt            em_metrics, em_outline;
      FT_TS_Bool            scaling;


      scaling = FT_TS_BOOL( !( load_flags & FT_TS_LOAD_NO_SCALE ) );

      /* copy outline data */
      *outline = slot->glyph.loader->base.outline;

      outline->flags &= ~FT_TS_OUTLINE_OWNER;
      outline->flags |= FT_TS_OUTLINE_REVERSE_FILL;

      if ( pfrsize->metrics.y_ppem < 24 )
        outline->flags |= FT_TS_OUTLINE_HIGH_PRECISION;

      /* compute the advance vector */
      metrics->horiAdvance = 0;
      metrics->vertAdvance = 0;

      advance    = gchar->advance;
      em_metrics = face->phy_font.metrics_resolution;
      em_outline = face->phy_font.outline_resolution;

      if ( em_metrics != em_outline )
        advance = FT_TS_MulDiv( advance,
                             (FT_TS_Long)em_outline,
                             (FT_TS_Long)em_metrics );

      if ( face->phy_font.flags & PFR_PHY_VERTICAL )
        metrics->vertAdvance = advance;
      else
        metrics->horiAdvance = advance;

      pfrslot->linearHoriAdvance = metrics->horiAdvance;
      pfrslot->linearVertAdvance = metrics->vertAdvance;

      /* make up vertical metrics(?) */
      metrics->vertBearingX = 0;
      metrics->vertBearingY = 0;

#if 0 /* some fonts seem to be broken here! */

      /* Apply the font matrix, if any.                 */
      /* TODO: Test existing fonts with unusual matrix  */
      /* whether we have to adjust Units per EM.        */
      {
        FT_TS_Matrix font_matrix;


        font_matrix.xx = face->log_font.matrix[0] << 8;
        font_matrix.yx = face->log_font.matrix[1] << 8;
        font_matrix.xy = face->log_font.matrix[2] << 8;
        font_matrix.yy = face->log_font.matrix[3] << 8;

        FT_TS_Outline_Transform( outline, &font_matrix );
      }
#endif

      /* scale when needed */
      if ( scaling )
      {
        FT_TS_Int      n;
        FT_TS_Fixed    x_scale = pfrsize->metrics.x_scale;
        FT_TS_Fixed    y_scale = pfrsize->metrics.y_scale;
        FT_TS_Vector*  vec     = outline->points;


        /* scale outline points */
        for ( n = 0; n < outline->n_points; n++, vec++ )
        {
          vec->x = FT_TS_MulFix( vec->x, x_scale );
          vec->y = FT_TS_MulFix( vec->y, y_scale );
        }

        /* scale the advance */
        metrics->horiAdvance = FT_TS_MulFix( metrics->horiAdvance, x_scale );
        metrics->vertAdvance = FT_TS_MulFix( metrics->vertAdvance, y_scale );
      }

      /* compute the rest of the metrics */
      FT_TS_Outline_Get_CBox( outline, &cbox );

      metrics->width        = cbox.xMax - cbox.xMin;
      metrics->height       = cbox.yMax - cbox.yMin;
      metrics->horiBearingX = cbox.xMin;
      metrics->horiBearingY = cbox.yMax - metrics->height;
    }

  Exit:
    return error;
  }


  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****                      KERNING METHOD                           *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/

  FT_TS_LOCAL_DEF( FT_TS_Error )
  pfr_face_get_kerning( FT_TS_Face     pfrface,        /* PFR_Face */
                        FT_TS_UInt     glyph1,
                        FT_TS_UInt     glyph2,
                        FT_TS_Vector*  kerning )
  {
    PFR_Face     face     = (PFR_Face)pfrface;
    FT_TS_Error     error    = FT_TS_Err_Ok;
    PFR_PhyFont  phy_font = &face->phy_font;
    FT_TS_UInt32    code1, code2, pair;


    kerning->x = 0;
    kerning->y = 0;

    if ( glyph1 > 0 )
      glyph1--;

    if ( glyph2 > 0 )
      glyph2--;

    /* convert glyph indices to character codes */
    if ( glyph1 > phy_font->num_chars ||
         glyph2 > phy_font->num_chars )
      goto Exit;

    code1 = phy_font->chars[glyph1].char_code;
    code2 = phy_font->chars[glyph2].char_code;
    pair  = PFR_KERN_INDEX( code1, code2 );

    /* now search the list of kerning items */
    {
      PFR_KernItem  item   = phy_font->kern_items;
      FT_TS_Stream     stream = pfrface->stream;


      for ( ; item; item = item->next )
      {
        if ( pair >= item->pair1 && pair <= item->pair2 )
          goto FoundPair;
      }
      goto Exit;

    FoundPair: /* we found an item, now parse it and find the value if any */
      if ( FT_TS_STREAM_SEEK( item->offset )                       ||
           FT_TS_FRAME_ENTER( item->pair_count * item->pair_size ) )
        goto Exit;

      {
        FT_TS_UInt    count       = item->pair_count;
        FT_TS_UInt    size        = item->pair_size;
        FT_TS_UInt    power       = 1 << FT_TS_MSB( count );
        FT_TS_UInt    probe       = power * size;
        FT_TS_UInt    extra       = count - power;
        FT_TS_Byte*   base        = stream->cursor;
        FT_TS_Bool    twobytes    = FT_TS_BOOL( item->flags & PFR_KERN_2BYTE_CHAR );
        FT_TS_Bool    twobyte_adj = FT_TS_BOOL( item->flags & PFR_KERN_2BYTE_ADJ  );
        FT_TS_Byte*   p;
        FT_TS_UInt32  cpair;


        if ( extra > 0 )
        {
          p = base + extra * size;

          if ( twobytes )
            cpair = FT_TS_NEXT_ULONG( p );
          else
            cpair = PFR_NEXT_KPAIR( p );

          if ( cpair == pair )
            goto Found;

          if ( cpair < pair )
          {
            if ( twobyte_adj )
              p += 2;
            else
              p++;
            base = p;
          }
        }

        while ( probe > size )
        {
          probe >>= 1;
          p       = base + probe;

          if ( twobytes )
            cpair = FT_TS_NEXT_ULONG( p );
          else
            cpair = PFR_NEXT_KPAIR( p );

          if ( cpair == pair )
            goto Found;

          if ( cpair < pair )
            base += probe;
        }

        p = base;

        if ( twobytes )
          cpair = FT_TS_NEXT_ULONG( p );
        else
          cpair = PFR_NEXT_KPAIR( p );

        if ( cpair == pair )
        {
          FT_TS_Int  value;


        Found:
          if ( twobyte_adj )
            value = FT_TS_PEEK_SHORT( p );
          else
            value = p[0];

          kerning->x = item->base_adj + value;
        }
      }

      FT_TS_FRAME_EXIT();
    }

  Exit:
    return error;
  }


/* END */
