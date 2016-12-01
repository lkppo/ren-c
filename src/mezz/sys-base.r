REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "REBOL 3 Boot Sys: Top Context Functions"
    Rights: {
        Copyright 2012 REBOL Technologies
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
    Context: sys
    Note: {
        Follows the BASE lib init that provides a basic set of functions
        to be able to evaluate this code.

        The boot binding of this module is SYS then LIB deep.
        Any non-local words not found in those contexts WILL BE
        UNBOUND and will error out at runtime!
    }
]

;-- SYS context definition begins here --
;   WARNING: ORDER DEPENDENT part of context (accessed from C code)

native: _ ; for boot only
action: _ ; for boot only

do*: function [
    {SYS: Called by system for DO on datatypes that require special handling.}
    return: [<opt> any-value!]
    value [file! url! string! binary! tag!]
    args [logic!]
        "Positional workaround of /ARGS"
    arg [any-value!]
        "Args passed as system/script/args to a script (normally a string)"
    next [logic!]
        "Positional workaround of /NEXT"
    var [blank! word!]
        "If do next expression only, variable updated with new block position"
][
    ; !!! These were refinements on the original DO* which were called from
    ; the system using positional order.  Under the Ren-C model you cannot
    ; select refinements positionally, nor can you pass "void" cells.  It
    ; would be *possible* to keep these going as refinements and have the
    ; system build a path to make a call, but this is easier.  Revisit this
    ; (also revisit the use of the word "next")
    ;
    arg: either args [arg] [void]
    var: either next [var] [void]

    ; This code is only called for urls, files, strings, and tags.
    ; DO of functions, blocks, paths, and other do-able types is done in the
    ; native, and this code is not called.
    ; Note that DO of file path evaluates in the directory of the target file.
    ; Files, urls and modules evaluate as scripts, other strings don't.
    ; Note: LOAD/header returns a block with the header object in the first
    ;       position, or will cause an error. No exceptions, not even for
    ;       directories or media.
    ;       Currently, load of URL has no special block forms.

    ; !!! DEMONSTRATION OF CONCEPT... this translates a tag into a URL!, but
    ; it should be using a more "official" URL instead of on individuals
    ; websites.  There should also be some kind of local caching facility.
    ;
    if tag? value [
        if value = <r3-legacy> [
            ; Special compatibility tag... Rebol2 and R3-Alpha will ignore the
            ; DO of a <tag>, so this is a no-op in them.
            ;
            return r3-legacy* ;-- defined in %mezz-legacy.r
        ]

        ; Convert value into a URL!
        value: switch/default value [
            ; Encodings and data formats
            <json> [http://reb4.me/r3/json.reb]
            <xml> [http://reb4.me/r3/altxml.reb]

            ; Web services
            <amazon-s3> [http://reb4.me/r3/s3.reb]
            <twitter> [https://raw.githubusercontent.com/gchiu/rebolbot/master/twitter.r3]
            <trello> [http://codeconscious.com/rebol-scripts/trello.r]

            ; Dialects
            <rebmu> [https://raw.githubusercontent.com/hostilefork/rebmu/master/rebmu.reb]
        ][
            fail [
                {Module} value {not in "rebol.org index" (hardcoded for now)}
            ]
        ]
    ]

    original-path: what-dir

    ; If a file is being mentioned as a DO location and the "current path"
    ; is a URL!, then adjust the value to be a URL! based from that path.
    if all [url? original-path  file? value] [
         value: join-of original-path value
    ]

    ; Load the data, first so it will error before change-dir
    data: load/header/type value 'unbound ; unbound so DO-NEEDS runs before INTERN
    ; Get the header and advance 'data to the code position
    hdr: first+ data  ; object or blank
    ; data is a block! here, with the header object in the first position back
    is-module: 'module = select hdr 'type

    either all [string? value  not is-module] [
        ; Return result without script overhead
        do-needs hdr  ; Load the script requirements
        if empty? data [if next [set var data] return ()]
        intern data   ; Bind the user script
        catch/quit [do/next data :var]
    ][ ; Otherwise we are in script mode

        ; When we run a script, the "current" directory is changed to the
        ; directory of that script.  This way, relative path lookups to
        ; find dependent files will look relative to the script.
        ;
        ; We want this behavior for both FILE! and for URL!, which means
        ; that the "current" path may become a URL!.  This can be processed
        ; with change-dir commands, but it will be protocol dependent as
        ; to whether a directory listing would be possible (HTTP does not
        ; define a standard for that)
        ;
        if all [
            any [file? value  url? value]
            file: find/last/tail value slash
        ][
            change-dir copy/part value file
        ]

        ; Make the new script object
        scr: system/script  ; and save old one
        system/script: construct system/standard/script [
            title: select hdr 'title
            header: hdr
            parent: :scr
            path: what-dir
            args: to-value :arg
        ]

        ; Print out the script info
        boot-print [
            (either is-module "Module:" "Script:") select hdr 'title
                "Version:" opt select hdr 'version
                "Date:" opt select hdr 'date
        ]

        also (
            ; Eval the block or make the module, returned
            either is-module [ ; Import the module and set the var
                also (
                    import catch/quit [
                        module/mixin hdr data (opt do-needs/no-user hdr)
                    ]
                )(
                    if next [set var tail data]
                )
            ][
                do-needs hdr  ; Load the script requirements
                intern data   ; Bind the user script
                catch/quit [do/next data :var]
            ]
        )(
            ; Restore system/script and the dir
            system/script: :scr
            if original-path [change-dir original-path]
        )
    ]
]

; MOVE some of these to SYSTEM?
boot-banner: ajoin ["REBOL 3.0 A" system/version/3 " " system/build newline]
boot-help: "Boot-sys level - no extra features."
boot-host: _ ; any host add-ons to the lib (binary)
boot-mezz: _ ; built-in mezz code (put here on boot)
boot-prot: _ ; built-in boot protocols
boot-exts: _ ; boot extension list
boot-embedded: _ ; embedded script

export: func [
    "Low level export of values (e.g. functions) to lib."
    words [block!] "Block of words (already defined in local context)"
][
    for-each word words [join lib [word get word]]
]

assert-utf8: function [
    "If binary data is UTF-8, returns it, else throws an error."
    data [binary!]
][
    unless find [0 8] tmp: utf? data [ ; Not UTF-8
        cause-error 'script 'no-decode ajoin ["UTF-" abs tmp]
    ]
    data
]
