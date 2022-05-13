/*
 *
 */

#include "idaldr.h"
#include "types.h"
#include "exefs.h"
#include "ncch.h"

//--------------------------------------------------------------------------
//
//      check input file format. if recognized, then return 1
//      and fill 'fileformatname'.
//      otherwise return 0
//
static int idaapi accept_file(
        qstring *fileformatname,
        qstring *processor,
        linput_t *li,
        const char *)
{
    NCCH ncch;
    if (ncch.check_header(li)) {
        *fileformatname = "Nintendo 3DS game dump (NCCH/CXI)";
        *processor = "arm";
        return 1;
    }
    return 0;
}

//--------------------------------------------------------------------------
//
//      load file into the database.
//
static void idaapi load_file(linput_t *li, ushort neflag, const char * fileformatname)
{
    NCCH ncch;
    set_processor_type("ARM", SETPROC_LOADER);
    if (ncch.check_header(li) && qstrcmp(fileformatname, "Nintendo 3DS firmware dump")) {
        ncch.load_file(li);
    }
}

//----------------------------------------------------------------------
//
//      LOADER DESCRIPTION BLOCK
//
//----------------------------------------------------------------------
loader_t LDSC =
{
  IDP_INTERFACE_VERSION,
  0,                            // loader flags
//
//      check input file format. if recognized, then return 1
//      and fill 'fileformatname'.
//      otherwise return 0
//
  accept_file,
//
//      load file into the database.
//
  load_file,
//
//      create output file from the database.
//      this function may be absent.
//
  NULL,
//      take care of a moved segment (fix up relocations, for example)
  NULL,
  NULL,
};
