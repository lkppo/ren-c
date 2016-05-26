//
//  File: %d-print.c
//  Summary: "low-level console print interface"
//  Section: debug
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2016 Rebol Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//=////////////////////////////////////////////////////////////////////////=//
//
// R3 is intended to run on fairly minimal devices, so this code may
// duplicate functions found in a typical C lib. That's why output
// never uses standard clib printf functions.
//

/*
        Print_OS... - low level OS output functions
        Out_...     - general console output functions
        Debug_...   - debug mode (trace) output functions
*/

#include "sys-core.h"

static REBREQ *Req_SIO;


/***********************************************************************
**
**  Lower Level Print Interface
**
***********************************************************************/

//
//  Init_StdIO: C
//
void Init_StdIO(void)
{
    //OS_CALL_DEVICE(RDI_STDIO, RDC_INIT);
    Req_SIO = OS_MAKE_DEVREQ(RDI_STDIO);
    if (!Req_SIO) panic (Error(RE_IO_ERROR));

    // The device is already open, so this call will just setup
    // the request fields properly.
    OS_DO_DEVICE(Req_SIO, RDC_OPEN);
}


//
//  Shutdown_StdIO: C
//
void Shutdown_StdIO(void)
{
    // !!! There is no OS_FREE_DEVREQ.  Should there be?  Should this
    // include an OS_ABORT_DEVICE?
    OS_FREE(Req_SIO);
}


//
//  Print_OS_Line: C
// 
// Print a new line.
//
void Print_OS_Line(void)
{
    // !!! Don't put const literal directly into mutable Req_SIO->data
    static REBYTE newline[] = "\n";

    Req_SIO->common.data = newline;
    Req_SIO->length = 1;
    Req_SIO->actual = 0;

    OS_DO_DEVICE(Req_SIO, RDC_WRITE);

    if (Req_SIO->error) panic (Error(RE_IO_ERROR));
}


//
//  Prin_OS_String: C
// 
// Print a string (with no line terminator).
// 
// The encoding options are OPT_ENC_XXX flags OR'd together.
//
void Prin_OS_String(const void *p, REBCNT len, REBFLGS opts)
{
    #define BUF_SIZE 1024
    REBYTE buffer[BUF_SIZE]; // on stack
    REBYTE *buf = &buffer[0];
    REBCNT len2;
    const REBOOL unicode = LOGICAL(opts & OPT_ENC_UNISRC);

    const REBYTE *bp = unicode ? NULL : cast(const REBYTE *, p);
    const REBUNI *up = unicode ? cast(const REBUNI *, p) : NULL;

    if (!p) panic (Error(RE_NO_PRINT_PTR));

    // Determine length if not provided:
    if (len == UNKNOWN) len = unicode ? Strlen_Uni(up) : LEN_BYTES(bp);

    SET_FLAG(Req_SIO->flags, RRF_FLUSH);

    Req_SIO->actual = 0;
    Req_SIO->common.data = buf;
    buffer[0] = 0; // for debug tracing

    if (opts & OPT_ENC_RAW) {
        //
        // !!! See notes on other invocations about the questions raised by
        // calls to Do_Signals_Throws() by places that do not have a clear
        // path up to return results from an interactive breakpoint.
        //
        REBVAL result;
        VAL_INIT_WRITABLE_DEBUG(&result);

        if (Do_Signals_Throws(&result))
            fail (Error_No_Catch_For_Throw(&result));
        if (IS_ANY_VALUE(&result))
            fail (Error(RE_MISC));

        // Used by verbatim terminal output, e.g. print of a BINARY!
        assert(!unicode);
        Req_SIO->length = len;

        // Mutability cast, but RDC_WRITE should not be modifying the buffer
        // (doing so could yield undefined behavior)
        Req_SIO->common.data = m_cast(REBYTE *, bp);

        OS_DO_DEVICE(Req_SIO, RDC_WRITE);
        if (Req_SIO->error) panic (Error(RE_IO_ERROR));
    }
    else {
        while ((len2 = len) > 0) {
            //
            // !!! See notes on other invocations about the questions raised by
            // calls to Do_Signals_Throws() by places that do not have a clear
            // path up to return results from an interactive breakpoint.
            //
            REBVAL result;
            VAL_INIT_WRITABLE_DEBUG(&result);

            if (Do_Signals_Throws(&result))
                fail (Error_No_Catch_For_Throw(&result));
            if (IS_ANY_VALUE(&result))
                fail (Error(RE_MISC));

            Req_SIO->length = Encode_UTF8(
                buf,
                BUF_SIZE - 4,
                unicode ? cast(const void *, up) : cast(const void *, bp),
                &len2,
                opts
            );

            if (unicode) up += len2; else bp += len2;
            len -= len2;

            OS_DO_DEVICE(Req_SIO, RDC_WRITE);
            if (Req_SIO->error) panic (Error(RE_IO_ERROR));
        }
    }
}


//
//  Out_Value: C
//
void Out_Value(const REBVAL *value, REBCNT limit, REBOOL mold, REBINT lines)
{
    Print_Value(value, limit, mold); // higher level!
    for (; lines > 0; lines--) Print_OS_Line();
}


//
//  Out_Str: C
//
void Out_Str(const REBYTE *bp, REBINT lines)
{
    Prin_OS_String(bp, UNKNOWN, OPT_ENC_CRLF_MAYBE);
    for (; lines > 0; lines--) Print_OS_Line();
}


/***********************************************************************
**
**  Debug Print Interface
**
**      If the Trace_Buffer exists, then output goes there,
**      otherwise output goes to OS output.
**
***********************************************************************/


//
//  Enable_Backtrace: C
//
void Enable_Backtrace(REBOOL on)
{
    if (on) {
        if (Trace_Limit == 0) {
            Trace_Limit = 100000;
            Trace_Buffer = Make_Binary(Trace_Limit);
        }
    }
    else {
        if (Trace_Limit) Free_Series(Trace_Buffer);
        Trace_Limit = 0;
        Trace_Buffer = 0;
    }
}


//
//  Display_Backtrace: C
//
void Display_Backtrace(REBCNT lines)
{
    REBCNT tail;
    REBCNT i;

    if (Trace_Limit > 0) {
        tail = SER_LEN(Trace_Buffer);
        i = tail - 1;
        for (lines++ ;lines > 0; lines--, i--) {
            i = Find_Str_Char(LF, Trace_Buffer, 0, i, tail, -1, 0);
            if (i == NOT_FOUND || i == 0) {
                i = 0;
                break;
            }
        }

        if (lines == 0) i += 2; // start of next line
        Prin_OS_String(BIN_AT(Trace_Buffer, i), tail - i, OPT_ENC_CRLF_MAYBE);
        //RESET_SERIES(Trace_Buffer);
    }
    else {
        Out_Str(cb_cast("backtrace not enabled"), 1);
    }
}


//
//  Debug_String: C
//
void Debug_String(const void *p, REBCNT len, REBOOL unicode, REBINT lines)
{
    REBUNI uni;
    const REBYTE *bp = unicode ? NULL : cast(const REBYTE *, p);
    const REBUNI *up = unicode ? cast(const REBUNI *, p) : NULL;

    REBINT disabled = GC_Disabled;
    GC_Disabled = 1;

    if (Trace_Limit > 0) {
        if (SER_LEN(Trace_Buffer) >= Trace_Limit)
            Remove_Series(Trace_Buffer, 0, 2000);

        if (len == UNKNOWN) len = unicode ? Strlen_Uni(up) : LEN_BYTES(bp);

        for (; len > 0; len--) {
            uni = unicode ? *up++ : *bp++;
            Append_Codepoint_Raw(Trace_Buffer, uni);
        }

        for (; lines > 0; lines--) Append_Codepoint_Raw(Trace_Buffer, LF);
        /* Append_Unencoded_Len(Trace_Buffer, bp, len); */ // !!! alternative?
    }
    else {
        Prin_OS_String(
            p, len, (unicode ? OPT_ENC_UNISRC : 0) | OPT_ENC_CRLF_MAYBE
        );
        for (; lines > 0; lines--) Print_OS_Line();
    }

    assert(GC_Disabled == 1);
    GC_Disabled = disabled;
}


//
//  Debug_Line: C
//
void Debug_Line(void)
{
    Debug_String(cb_cast(""), UNKNOWN, FALSE, 1);
}


//
//  Debug_Str: C
// 
// Print a string followed by a newline.
//
void Debug_Str(const char *str)
{
    Debug_String(cb_cast(str), UNKNOWN, FALSE, 1);
}


//
//  Debug_Uni: C
// 
// Print debug unicode string followed by a newline.
//
void Debug_Uni(REBSER *ser)
{
    const REBFLGS encopts = OPT_ENC_UNISRC | OPT_ENC_CRLF_MAYBE;
    REBCNT ul;
    REBCNT bl;
    REBYTE buf[1024];
    REBUNI *up = UNI_HEAD(ser);
    REBCNT size = SER_LEN(ser);

    REBINT disabled = GC_Disabled;
    GC_Disabled = 1;

    while (size > 0) {
        ul = size;
        bl = Encode_UTF8(buf, 1020, up, &ul, encopts);
        Debug_String(buf, bl, FALSE, 0);
        size -= ul;
        up += ul;
    }

    Debug_Line();

    assert(GC_Disabled == 1);
    GC_Disabled = disabled;
}


#if !defined(NDEBUG)

//
//  Debug_Series: C
//
void Debug_Series(REBSER *ser)
{
    REBINT disabled = GC_Disabled;
    GC_Disabled = 1;

    // Invalid series would possibly (but not necessarily) crash the print
    // routines--which are the same ones used to output a series normally.
    // Hence Debug_Series should not be used to attempt to print a known
    // malformed series.  ASSERT_SERIES will likely give a more pointed
    // message about what is wrong than just crashing the print code...

    ASSERT_SERIES(ser);

    // This routine is also a little catalog of the outlying series
    // types in terms of sizing, just to know what they are.

    if (BYTE_SIZE(ser))
        Debug_Str(s_cast(BIN_HEAD(ser)));
    else if (Is_Array_Series(ser)) {
        REBVAL value;
        VAL_INIT_WRITABLE_DEBUG(&value);

        // May not actually be a REB_BLOCK, but we put it in a value
        // container for now saying it is so we can output it.  It may be
        // a context and we may not want to Manage_Series here, so we use a
        // raw VAL_SET instead of Val_Init_Block
        //
        VAL_RESET_HEADER(&value, REB_BLOCK);
        INIT_VAL_SERIES(&value, ser);
        VAL_INDEX(&value) = 0;

        Debug_Fmt("%r", &value);
    }
    else if (SER_WIDE(ser) == sizeof(REBUNI))
        Debug_Uni(ser);
    else if (ser == Bind_Table) {
        // Dump bind table somehow?
        Panic_Series(ser);
    } else if (ser == PG_Word_Table.hashes) {
        // Dump hashes somehow?
        Panic_Series(ser);
    } else if (ser == GC_Series_Guard) {
        // Dump protected series pointers somehow?
        Panic_Series(ser);
    } else if (ser == GC_Value_Guard) {
        // Dump protected value pointers somehow?
        Panic_Series(ser);
    }
    else
        Panic_Series(ser);

    assert(GC_Disabled == 1);
    GC_Disabled = disabled;
}

#endif


//
//  Debug_Num: C
// 
// Print a string followed by a number.
//
void Debug_Num(const REBYTE *str, REBINT num)
{
    REBYTE buf[40];

    Debug_String(str, UNKNOWN, FALSE, 0);
    Debug_String(cb_cast(" "), 1, FALSE, 0);
    Form_Hex_Pad(buf, num, 8);
    Debug_Str(s_cast(buf));
}


//
//  Debug_Chars: C
// 
// Print a number of spaces.
//
void Debug_Chars(REBYTE chr, REBCNT num)
{
    REBYTE spaces[100];

    memset(spaces, chr, MIN(num, 99));
    spaces[num] = 0;
    Debug_String(spaces, num, FALSE, 0);
}


//
//  Debug_Space: C
// 
// Print a number of spaces.
//
void Debug_Space(REBCNT num)
{
    if (num > 0) Debug_Chars(' ', num);
}


//
//  Debug_Word: C
// 
// Print a REBOL word.
//
void Debug_Word(const REBVAL *word)
{
    Debug_Str(cs_cast(Get_Sym_Name(VAL_WORD_SYM(word))));
}


//
//  Debug_Type: C
// 
// Print a REBOL datatype name.
//
void Debug_Type(const REBVAL *value)
{
    if (VAL_TYPE(value) < REB_MAX) Debug_Str(cs_cast(Get_Type_Name(value)));
    else Debug_Str("TYPE?!");
}


//
//  Debug_Value: C
//
void Debug_Value(const REBVAL *value, REBCNT limit, REBOOL mold)
{
    Print_Value(value, limit, mold); // higher level!
}


//
//  Debug_Values: C
//
void Debug_Values(const REBVAL *value, REBCNT count, REBCNT limit)
{
    REBCNT i1;
    REBCNT i2;
    REBUNI uc, pc = ' ';
    REBCNT n;

    for (n = 0; n < count; n++, value++) {
        Debug_Space(1);
        if (n > 0 && VAL_TYPE(value) <= REB_NONE) Debug_Chars('.', 1);
        else {
            REB_MOLD mo;
            CLEARS(&mo);
            if (limit != 0) {
                SET_FLAG(mo.opts, MOPT_LIMIT);
                mo.limit = limit;
            }
            Push_Mold(&mo);

            Mold_Value(&mo, value, TRUE);
            Throttle_Mold(&mo); // not using Pop_Mold(), must do explicitly

            for (i1 = i2 = mo.start; i1 < SER_LEN(mo.series); i1++) {
                uc = GET_ANY_CHAR(mo.series, i1);
                if (uc < ' ') uc = ' ';
                if (uc > ' ' || pc > ' ') SET_ANY_CHAR(mo.series, i2++, uc);
                pc = uc;
            }
            SET_ANY_CHAR(mo.series, i2, '\0');
            assert(SER_WIDE(mo.series) == sizeof(REBUNI));
            Debug_String(
                UNI_AT(mo.series, mo.start),
                i2 - mo.start,
                TRUE,
                0
            );

            Drop_Mold(&mo);
        }
    }
    Debug_Line();
}


//
//  Debug_Buf: C
// 
// (va_list by pointer: http://stackoverflow.com/a/3369762/211160)
// 
// Lower level formatted print for debugging purposes.
// 
// 1. Does not support UNICODE.
// 2. Does not auto-expand the output buffer.
// 3. No termination buffering (limited length).
// 
// Print using a format string and variable number
// of arguments.  All args must be long word aligned
// (no short or char sized values unless recast to long).
// 
// Output will be held in series print buffer and
// will not exceed its max size.  No line termination
// is supplied after the print.
//
void Debug_Buf(const char *fmt, va_list *vaptr)
{
    REBCNT len;
    REBCNT sub;
    REBCNT n;
    REBSER *bytes;
    REBINT disabled = GC_Disabled;
    REB_MOLD mo;
    CLEARS(&mo);

    GC_Disabled = 1;

    Push_Mold(&mo);

    Form_Args_Core(&mo, fmt, vaptr);

    bytes = Pop_Molded_UTF8(&mo);

    // Don't send the Debug_String routine more than 1024 bytes at a time,
    // but chunk it to 1024 byte sections.
    //
    // !!! What's the rationale for this?
    //
    len = SER_LEN(bytes);
    for (n = 0; n < len; n += sub) {
        sub = len - n;
        if (sub > 1024) sub = 1024;
        Debug_String(BIN_AT(bytes, n), sub, FALSE, 0);
    }

    Free_Series(bytes);

    assert(GC_Disabled == 1);
    GC_Disabled = disabled;
}


//
//  Debug_Fmt_: C
// 
// Print using a format string and variable number
// of arguments.  All args must be long word aligned
// (no short or char sized values unless recast to long).
// Output will be held in series print buffer and
// will not exceed its max size.  No line termination
// is supplied after the print.
//
void Debug_Fmt_(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    Debug_Buf(fmt, &va);
    va_end(va);
}


//
//  Debug_Fmt: C
// 
// Print using a formatted string and variable number
// of arguments.  All args must be long word aligned
// (no short or char sized values unless recast to long).
// Output will be held in a series print buffer and
// will not exceed its max size.  A line termination
// is supplied after the print.
//
void Debug_Fmt(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Debug_Buf(fmt, &args);
    Debug_Line();
    va_end(args);
}


//
//  Echo_File: C
//
REBOOL Echo_File(REBCHR *file)
{
    Req_SIO->special.file.path = file;
    return LOGICAL(DR_ERROR != OS_DO_DEVICE(Req_SIO, RDC_CREATE));
}


//
//  Form_Hex_Pad: C
// 
// Form an integer hex string in the given buffer with a
// width padded out with zeros.
// If len = 0 and val = 0, a null string is formed.
// Does not insert a #.
// Make sure you have room in your buffer before calling this!
//
REBYTE *Form_Hex_Pad(REBYTE *buf, REBI64 val, REBINT len)
{
    REBYTE buffer[MAX_HEX_LEN+4];
    REBYTE *bp = buffer + MAX_HEX_LEN + 1;
    REBI64 sgn;

    // !!! val parameter was REBI64 at one point; changed to REBI64
    // as this does signed comparisons (val < 0 was never true...)
    sgn = (val < 0) ? -1 : 0;

    len = MIN(len, MAX_HEX_LEN);
    *bp-- = 0;
    while (val != sgn && len > 0) {
        *bp-- = Hex_Digits[val & 0xf];
        val >>= 4;
        len--;
    }
    for (; len > 0; len--) *bp-- = (sgn != 0) ? 'F' : '0';
    bp++;
    while ((*buf++ = *bp++));
    return buf-1;
}


//
//  Form_Hex2: C
// 
// Convert byte-sized int to xx format. Very fast.
//
REBYTE *Form_Hex2(REBYTE *bp, REBCNT val)
{
    bp[0] = Hex_Digits[(val & 0xf0) >> 4];
    bp[1] = Hex_Digits[val & 0xf];
    bp[2] = 0;
    return bp+2;
}


//
//  Form_Hex2_Uni: C
// 
// Convert byte-sized int to unicode xx format. Very fast.
//
REBUNI *Form_Hex2_Uni(REBUNI *up, REBCNT val)
{
    up[0] = Hex_Digits[(val & 0xf0) >> 4];
    up[1] = Hex_Digits[val & 0xf];
    up[2] = 0;
    return up+2;
}


//
//  Form_Hex_Esc_Uni: C
// 
// Convert byte int to %xx format (in unicode destination)
//
REBUNI *Form_Hex_Esc_Uni(REBUNI *up, REBUNI c)
{
    up[0] = '%';
    up[1] = Hex_Digits[(c & 0xf0) >> 4];
    up[2] = Hex_Digits[c & 0xf];
    up[3] = 0;
    return up+3;
}


//
//  Form_RGB_Uni: C
// 
// Convert 24 bit RGB to xxxxxx format.
//
REBUNI *Form_RGB_Uni(REBUNI *up, REBCNT val)
{
#ifdef ENDIAN_LITTLE
    up[0] = Hex_Digits[(val >>  4) & 0xf];
    up[1] = Hex_Digits[val & 0xf];
    up[2] = Hex_Digits[(val >> 12) & 0xf];
    up[3] = Hex_Digits[(val >>  8) & 0xf];
    up[4] = Hex_Digits[(val >> 20) & 0xf];
    up[5] = Hex_Digits[(val >> 16) & 0xf];
#else
    up[0] = Hex_Digits[(val >>  28) & 0xf];
    up[1] = Hex_Digits[(val >> 24) & 0xf];
    up[2] = Hex_Digits[(val >> 20) & 0xf];
    up[3] = Hex_Digits[(val >> 16) & 0xf];
    up[4] = Hex_Digits[(val >> 12) & 0xf];
    up[5] = Hex_Digits[(val >>  8) & 0xf];
#endif
    up[6] = 0;

    return up+6;
}


//
//  Form_Uni_Hex: C
// 
// Fast var-length hex output for uni-chars.
// Returns next position (just past the insert).
//
REBUNI *Form_Uni_Hex(REBUNI *out, REBCNT n)
{
    REBUNI buffer[10];
    REBUNI *up = &buffer[10];

    while (n != 0) {
        *(--up) = Hex_Digits[n & 0xf];
        n >>= 4;
    }

    while (up < &buffer[10]) *out++ = *up++;

    return out;
}


//
//  Form_Args_Core: C
// 
// (va_list by pointer: http://stackoverflow.com/a/3369762/211160)
//
// This is an internal routine used for debugging, which is something like
// `printf` (it understands %d, %s, %c) but stripped down in features.
// It also knows how to show REBVAL* values FORMed (%v) or MOLDed (%r),
// as well as REBSER* or REBARR* series molded (%m).
//
// Initially it was considered to be for low-level debug output only.  It
// was strictly ASCII, and it only supported a fixed-size output destination
// buffer.  The buffer that it used was reused by other routines, and
// nested calls would erase the content.  The choice was made to use the
// implementation techniques of MOLD and the "mold stack"...allowing nested
// calls and unicode support.  It simplified the code, at the cost of
// becoming slightly more "bootstrapped".
//
void Form_Args_Core(REB_MOLD *mo, const char *fmt, va_list *vaptr)
{
    REBYTE *cp;
    REBINT pad;
    REBYTE desc;
    REBVAL value;
    REBYTE padding;
    REBSER *ser = mo->series;
    REBYTE buf[MAX_SCAN_DECIMAL];

    // buffer used for making byte-oriented renderings to add to the REBUNI
    // mold series.  Should be more formally checked as it's used for
    // integers, hex, eventually perhaps other things.
    //
    assert(MAX_SCAN_DECIMAL >= MAX_HEX_LEN);

    for (; *fmt != '\0'; fmt++) {

        // Copy format string until next % escape
        //
        while ((*fmt != '\0') && (*fmt != '%'))
            Append_Codepoint_Raw(ser, *fmt++);

        if (*fmt != '%') break;

        pad = 1;
        padding = ' ';
        fmt++; // skip %

pick:
        switch (desc = *fmt) {

        case '0':
            padding = '0';
        case '-':
        case '1':   case '2':   case '3':   case '4':
        case '5':   case '6':   case '7':   case '8':   case '9':
            fmt = cs_cast(Grab_Int(cb_cast(fmt), &pad));
            goto pick;

        case 'D':
            assert(FALSE); // !!! was identical code to "d"...why "D"?
        case 'd':
            // All va_arg integer arguments will be coerced to platform 'int'
            cp = Form_Int_Pad(
                buf,
                cast(REBI64, va_arg(*vaptr, int)),
                MAX_SCAN_DECIMAL,
                pad,
                padding
            );
            Append_Unencoded_Len(ser, s_cast(buf), cast(REBCNT, cp - buf));
            break;

        case 's':
            cp = va_arg(*vaptr, REBYTE *);
            if (pad == 1) pad = LEN_BYTES(cp);
            if (pad < 0) {
                pad = -pad;
                pad -= LEN_BYTES(cp);
                for (; pad > 0; pad--) Append_Codepoint_Raw(ser, ' ');
            }
            Append_Unencoded(ser, s_cast(cp));

            // !!! see R3-Alpha for original pad logic, this is an attempt
            // to make the output somewhat match without worrying heavily
            // about the padding features of this debug routine.
            //
            pad -= LEN_BYTES(cp);

            for (; pad > 0; pad--) Append_Codepoint_Raw(ser, ' ');
            break;

        case 'r':   // use Mold
        case 'v':   // use Form
            Mold_Value(
                mo,
                va_arg(*vaptr, const REBVAL*),
                LOGICAL(desc != 'v')
            );

            // !!! This used to "filter out ctrl chars", which isn't a bad
            // idea as a mold option (MOPT_FILTER_CTRL) but it would involve
            // some doing, as molding doesn't have a real "moment" that
            // it can always filter...since sometimes the buffer is examined
            // directly by clients vs. getting handed back.
            //
            /* for (; l > 0; l--, bp++) if (*bp < ' ') *bp = ' '; */
            break;

        case 'm':  // Mold a series
            // Val_Init_Block would Ensure_Series_Managed, we use a raw
            // VAL_SET instead.
            //
            // !!! Better approach?  Can the series be passed directly?
            //
            VAL_INIT_WRITABLE_DEBUG(&value);
            VAL_RESET_HEADER(&value, REB_BLOCK);
            INIT_VAL_SERIES(&value, va_arg(*vaptr, REBSER*));
            VAL_INDEX(&value) = 0;
            Mold_Value(mo, &value, TRUE);
            break;

        case 'c':
            Append_Codepoint_Raw(
                ser,
                cast(REBYTE, va_arg(*vaptr, REBINT))
            );
            break;

        case 'x':
            Append_Codepoint_Raw(ser, '#');
            if (pad == 1) pad = 8;
            cp = Form_Hex_Pad(
                buf,
                cast(REBU64, cast(REBUPT, va_arg(*vaptr, REBYTE*))),
                pad
            );
            Append_Unencoded_Len(ser, s_cast(buf), cp - buf);
            break;

        default:
            Append_Codepoint_Raw(ser, *fmt);
        }
    }

    TERM_SERIES(ser);
}


//
//  Form_Args: C
//
void Form_Args(REB_MOLD *mo, const char *fmt, ...)
{
    REBYTE *result;
    va_list args;

    va_start(args, fmt);
    Form_Args_Core(mo, fmt, &args);
    va_end(args);
}


/***********************************************************************
**
**  User Output Print Interface
**
***********************************************************************/

static const REBVAL *Pending_Format_Delimiter(
    const REBVAL* delimiter,
    REBCNT depth
) {
    if (!IS_BLOCK(delimiter))
        return delimiter;

    if (VAL_ARRAY_LEN_AT(delimiter) == 0)
        return NONE_VALUE;

    if (depth >= VAL_ARRAY_LEN_AT(delimiter))
        depth = VAL_ARRAY_LEN_AT(delimiter) - 1;

    // We allow the block passed in to not be at the head, so we have to
    // account for the index and offset from it.
    //
    delimiter = VAL_ARRAY_AT_HEAD(delimiter, VAL_INDEX(delimiter) + depth);

    // BAR! at the end signals a stop, not to keep repeating the last
    // delimiter for higher levels.  Same effect as a NONE!
    //
    if (IS_BAR(delimiter) || IS_NONE(delimiter))
        return NONE_VALUE;

    // We have to re-type-check against the legal delimiter types here.
    //
    if (ANY_STRING(delimiter) || IS_CHAR(delimiter))
        return delimiter;

    // !!! TBD: Error message, more comprehensive type check
    //
    fail (Error(RE_MISC));
}


//
//  Format_GC_Safe_Value_Throws: C
//
// This implements new logic (which should ultimately also power FAIL, FORM,
// and other print-like things).  The dialect will recurse on evaluated
// blocks, treat literal blocks as grouping, and a BAR! `|` signals a
// newline (as well as acting as an expression barrier)
//
REBOOL Format_GC_Safe_Value_Throws(
    REBVAL *out,
    REB_MOLD *mold,
    REBVAL *pending_delimiter,
    const REBVAL *val_gc_safe,
    REBOOL reduce,
    const REBVAL *delimiter,
    REBCNT depth
) {
    struct Reb_Frame f;

    if (IS_BLOCK(val_gc_safe)) {
        f.value = VAL_ARRAY_AT(val_gc_safe);
        if (IS_END(f.value))
            return FALSE; // no mold output added on empty blocks

        f.indexor = VAL_INDEX(val_gc_safe) + 1;
        f.source.array = VAL_ARRAY(val_gc_safe);
    }
    else {
        // Prefetch with value + an empty array to use same code path

        f.value = val_gc_safe;
        f.indexor = 0;
        f.source.array = EMPTY_ARRAY;
    }

    f.flags = DO_FLAG_NEXT | DO_FLAG_ARGS_EVALUATE | DO_FLAG_LOOKAHEAD;
    f.eval_fetched = NULL;

    do {
        if (IS_VOID(f.value)) {
            //
            // Unsets format to nothing (!!! should NONE! do the same?)
            //
            FETCH_NEXT_ONLY_MAYBE_END(&f);
        }
        else if (IS_BAR(f.value)) {
            //
            // !!! At each depth BAR! is always a barrier.  However, it may
            // also mean inserting a delimiter.  The default is to assume it
            // means to insert a newline at the outermost level and then
            // spaces at every inner level.  This should be overrideable
            // via something like a /bar refinement.

            if (depth == 0)
                Append_Codepoint_Raw(mold->series, '\n');
            else
                Append_Codepoint_Raw(mold->series, ' ');

            SET_END(pending_delimiter);
            FETCH_NEXT_ONLY_MAYBE_END(&f);
        }
        else if (IS_CHAR(f.value)) {
            //
            // Characters are inserted with no spacing.  This is because the
            // cases in which spaced-out-characters are most likely to be
            // interesting are cases that are probably debug-oriented in
            // which case MOLD should be used anyway.

            assert(
                SER_WIDE(mold->series) == sizeof(f.value->payload.character)
            );
            Append_Codepoint_Raw(mold->series, f.value->payload.character);

            SET_END(pending_delimiter); // no delimiting before/after chars
            FETCH_NEXT_ONLY_MAYBE_END(&f);
        }
        else if (IS_BINARY(f.value)) {
            //
            // Rather than introduce Rebol's specialized MOLD notation for
            // BINARY! into ordinary printing, the assumption is that it
            // should be interpreted as UTF-8 bytes.
            //
            if (VAL_LEN_AT(f.value) > 0) {
                if (!IS_END(pending_delimiter) && !IS_NONE(pending_delimiter))
                    Mold_Value(mold, pending_delimiter, FALSE);

                Append_UTF8_May_Fail(
                    mold->series, VAL_BIN_AT(f.value), VAL_LEN_AT(f.value)
                );

                *pending_delimiter
                    = *Pending_Format_Delimiter(delimiter, depth);
            }

            FETCH_NEXT_ONLY_MAYBE_END(&f);
        }
        else if (IS_BLOCK(f.value)) {
            //
            // If an expression was a literal block in the print source
            // then recurse and consider it a new depth level--using
            // the same reducing logic as the outermost level.  Note that
            // if it *evaluates* to a block, it will be output without
            // evaluation...just printed inertly.
            //
            // !!! Note that this literal examination of f.value does not
            // combine well with infix operators, if PRINT is considered to
            // allow any evaluation.  It would be recursing into something
            // like `print [[a] + [b]]` before it took the + into account.
            // One possibility would be to have all non-trivial evaluations
            // (e.g. things other than word lookup) be in a GROUP!.

            REBOOL nested_reduce = reduce;

            REBVAL maybe_thrown;
            VAL_INIT_WRITABLE_DEBUG(&maybe_thrown);

        #if !defined(NDEBUG)
            if (LEGACY(OPTIONS_NO_REDUCE_NESTED_PRINT))
                nested_reduce = FALSE;
        #endif

            // since the value we're iterating is guarded, f.value is too

            if (Format_GC_Safe_Value_Throws(
                &maybe_thrown, // not interested in value unless thrown
                mold,
                pending_delimiter,
                f.value,
                nested_reduce,
                delimiter,
                depth + 1
            )) {
                *out = maybe_thrown;
                return TRUE;
            }

            // If there's a delimiter pending (even a NONE!), then convert
            // it back to pending the delimiter of the *outer* element.
            //
            if (!IS_END(pending_delimiter)) {
                *pending_delimiter = *Pending_Format_Delimiter(
                    delimiter, depth
                );
            }

            FETCH_NEXT_ONLY_MAYBE_END(&f);
        }
        else if (reduce) {
            DO_NEXT_REFETCH_MAY_THROW(out, &f, DO_FLAG_LOOKAHEAD);
            if (f.indexor == THROWN_FLAG)
                return TRUE;

            // If we got here via a reduction step, we might have gotten
            // a BINARY! or a BAR! or some other type.  Don't call MOLD
            // directly because it won't necessarily do what this routine
            // wants...recurse with reduce=FALSE to pick those up.

            PUSH_GUARD_VALUE(out);
            Format_GC_Safe_Value_Throws(
                NULL, // can't throw--no need for output slot
                mold,
                pending_delimiter,
                out, // this level's output is recursion's input
                FALSE, // don't reduce the value again
                delimiter,
                depth // not nested block so no depth increment
            );
            DROP_GUARD_VALUE(out);

            // If there's a delimiter pending (even a NONE!), then convert
            // it back to pending the delimiter of the *outer* element.
            //
            if (!IS_END(pending_delimiter)) {
                *pending_delimiter
                    = *Pending_Format_Delimiter(delimiter, depth);
            }

            // The DO_NEXT already refetched...
        }
        else {
            // This is where the recursion bottoms out...the need to FORM
            // a terminal value.  We don't know in advance if the forming
            // will produce output, and if it doesn't we suppress the
            // delimiter...so to do that, we have to roll back.

            REBCNT rollback_point = UNI_LEN(mold->series);
            REBCNT mold_point;

            if (!IS_END(pending_delimiter) && !IS_NONE(pending_delimiter))
                Mold_Value(mold, pending_delimiter, FALSE);

            mold_point = UNI_LEN(mold->series);

            Mold_Value(mold, f.value, FALSE);

            if (UNI_LEN(mold->series) == mold_point) {
                //
                // The mold didn't add anything, so roll back and don't
                // update the pending delimiter.
                //
                SET_UNI_LEN(mold->series, rollback_point);
                UNI_TERM(mold->series);
                SET_NONE(pending_delimiter);
            }
            else {
                *pending_delimiter
                    = *Pending_Format_Delimiter(delimiter, depth);
            }

            FETCH_NEXT_ONLY_MAYBE_END(&f);
        }

        if (f.indexor == END_FLAG)
            break;
    } while (TRUE);

    return FALSE;
}


//
//  Prin_GC_Safe_Value_Throws: C
// 
// Print a value or block's contents for user viewing.
// Can limit output to a given size. Set limit to 0 for full size.
//
REBOOL Prin_GC_Safe_Value_Throws(
    REBVAL *out_if_reduce,
    const REBVAL *value,
    const REBVAL *delimiter,
    REBCNT limit,
    REBOOL mold,
    REBOOL reduce
) {
    REBVAL pending_delimiter;

    REB_MOLD mo;
    CLEARS(&mo);
    if (limit != 0) {
        SET_FLAG(mo.opts, MOPT_LIMIT);
        mo.limit = limit;
    }
    Push_Mold(&mo);

    VAL_INIT_WRITABLE_DEBUG(&pending_delimiter);
    SET_END(&pending_delimiter);

    if (mold)
        Mold_Value(&mo, value, TRUE);
    else {
        if (Format_GC_Safe_Value_Throws(
            out_if_reduce,
            &mo,
            &pending_delimiter,
            value,
            LOGICAL(reduce && IS_BLOCK(value)), // `print 'word` won't GET it
            delimiter,
            0 // depth
        )) {
            return TRUE;
        }
    }

    Throttle_Mold(&mo); // not using Pop_Mold(), must do explicitly

    assert(SER_WIDE(mo.series) == sizeof(REBUNI));
    Prin_OS_String(
        UNI_AT(mo.series, mo.start),
        SER_LEN(mo.series) - mo.start,
        OPT_ENC_UNISRC | OPT_ENC_CRLF_MAYBE
    );

    Drop_Mold(&mo);
    return FALSE;
}


//
//  Print_Value: C
// 
// Print a value or block's contents for user viewing.
// Can limit output to a given size. Set limit to 0 for full size.
//
void Print_Value(const REBVAL *value, REBCNT limit, REBOOL mold)
{
    // Note: Does not reduce
    //
    REBVAL delimiter;
    VAL_INIT_WRITABLE_DEBUG(&delimiter);
    SET_CHAR(&delimiter, ' ');

    (void)Prin_GC_Safe_Value_Throws(
        NULL, value, &delimiter, limit, mold, FALSE
    );

    Print_OS_Line();
}


//
//  Init_Raw_Print: C
// 
// Initialize print module.
//
void Init_Raw_Print(void)
{
    Set_Root_Series(TASK_BYTE_BUF,  Make_Binary(1000));
}
