//
//  File: %sys-protect.h
//  Summary: "System Const and Protection Functions"
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2018 Rebol Open Source Contributors
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
//=////////////////////////////////////////////////////////////////////////=//
//
// R3-Alpha introduced the idea of "protected" series and variables.  Ren-C
// introduces a new form of read-only-ness that is not a bit on series, but
// rather bits on values.  This means that a value can be a read-only view of
// a series that is otherwise mutable.
//
// !!! Checking for read access was a somewhat half-baked feature in R3-Alpha,
// as heeding the protection bit had to be checked explicitly.  Many places in
// the code did not do the check.  While several bugs of that nature have
// been replaced in an ad-hoc fashion, a better solution would involve using
// C's `const` feature to locate points that needed to promote series access
// to be mutable, so it could be checked at compile-time.
//

inline static void FAIL_IF_READ_ONLY_CORE(
    const RELVAL *v,
    REBSPC *specifier
){
    assert(GET_CELL_FLAG(v, FIRST_IS_NODE));
    REBSER *s = SER(VAL_NODE(v));  // can be pairlist, varlist, etc.

    FAIL_IF_READ_ONLY_SER(s);

    if (GET_CELL_FLAG(v, CONST)) {
        DECLARE_LOCAL (specific);
        Derelativize(specific, v, specifier);
        fail (Error_Const_Value_Raw(specific));
    }
}

#define FAIL_IF_READ_ONLY(v) \
    FAIL_IF_READ_ONLY_CORE((v), SPECIFIED)



inline static bool Is_Array_Deeply_Frozen(REBARR *a) {
    return GET_SERIES_INFO(a, FROZEN);

    // should be frozen all the way down (can only freeze arrays deeply)
}

inline static void Deep_Freeze_Array(REBARR *a) {
    Protect_Series(
        SER(a),
        0, // start protection at index 0
        PROT_DEEP | PROT_SET | PROT_FREEZE
    );
    Uncolor_Array(a);
}

#define Is_Array_Shallow_Read_Only(a) \
    Is_Series_Read_Only(a)
