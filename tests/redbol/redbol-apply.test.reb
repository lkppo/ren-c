; functions/control/apply.r

(did redbol-apply: function [
    return: [<opt> any-value!]
    action [action!]
    block [block!]
    /only
][
    frame: make frame! :action
    params: parameters of :action
    using-args: true

    while [block: sync-invisibles block] [
        block: if only [
            arg: block/1
            try next block
        ] else [
            try evaluate/set block lit arg:
        ]

        if refinement? params/1 [
            using-args: did set (in frame second params/1) :arg
        ] else [
            if using-args [
                set* (in frame params/1) :arg
            ]
        ]

        params: try next params
    ]

    comment [
        ;
        ; Too many arguments was not a problem for R3-alpha's APPLY, it
        ; would evaluate them all even if not used by the function.  It
        ; may or may not be better to have it be an error.
        ;
        if not tail? block [
            fail "Too many arguments passed in R3-ALPHA-APPLY block."
        ]
    ]

    do frame  ; nulls are optionals
])

[#44 (
    error? trap [redbol-apply 'append/only [copy [a b] 'c]]
)]
(1 == redbol-apply :subtract [2 1])
(1 = (redbol-apply :- [2 1]))
(error? trap [redbol-apply func [a] [a] []])
(error? trap [redbol-apply/only func [a] [a] []])

; CC#2237
(error? trap [redbol-apply func [a] [a] [1 2]])
(error? trap [redbol-apply/only func [a] [a] [1 2]])

(error? redbol-apply :make [error! ""])

(/a = redbol-apply func [/a] [a] [true])
(_ = redbol-apply func [/a] [a] [false])
(_ = redbol-apply func [/a] [a] [])
(/a = redbol-apply/only func [/a] [a] [true])
; the word 'false
(/a = redbol-apply/only func [/a] [a] [false])
(_ == redbol-apply/only func [/a] [a] [])
(use [a] [a: true /a = redbol-apply func [/a] [a] [a]])
(use [a] [a: false _ == redbol-apply func [/a] [a] [a]])
(use [a] [a: false /a = redbol-apply func [/a] [a] ['a]])
(use [a] [a: false /a = redbol-apply func [/a] [a] [/a]])
(use [a] [a: false /a = redbol-apply/only func [/a] [a] [a]])
(group! == redbol-apply/only (specialize 'of [property: 'type]) [()])
([1] == head of redbol-apply :insert [copy [] [1] blank blank blank])
([1] == head of redbol-apply :insert [copy [] [1] blank blank false])
([[1]] == head of redbol-apply :insert [copy [] [1] blank blank true])
(action! == redbol-apply (specialize 'of [property: 'type]) [:print])
(get-word! == redbol-apply/only (specialize 'of [property: 'type]) [:print])

;-- #1760 --

(
    1 == eval func [] [redbol-apply does [] [return 1] 2]
)
(
    1 == eval func [] [redbol-apply func [a] [a] [return 1] 2]
)
(
    1 == eval func [] [redbol-apply does [] [return 1]]
)
(
    1 == eval func [] [redbol-apply func [a] [a] [return 1]]
)
(
    1 == eval func [] [redbol-apply func [a b] [a] [return 1 2]]
)
(
    1 == eval func [] [redbol-apply func [a b] [a] [2 return 1]]
)

(
    void? redbol-apply func [
        return: [<opt> any-value!]
        x [<opt> any-value!]
    ][
        get 'x
    ][
        void
    ]
)
(
    void? redbol-apply func [
        return: [<opt> any-value!]
        'x [<opt> any-value!]
    ][
        get 'x
    ][
        void
    ]
)
(
    void? redbol-apply func [
        return: [<opt> any-value!]
        x [<opt> any-value!]
    ][
        return get 'x
    ][
        void
    ]
)
(
    void? redbol-apply func [
        return: [<opt> any-value!]
        'x [<opt> any-value!]
    ][
        return get 'x
    ][
        void
    ]
)
(
    error? redbol-apply func ['x [<opt> any-value!]] [
        return get 'x
    ][
        make error! ""
    ]
)
(
    error? redbol-apply/only func [x [<opt> any-value!]] [
        return get 'x
    ] head of insert copy [] make error! ""
)
(
    error? redbol-apply/only func ['x [<opt> any-value!]] [
        return get 'x
    ] head of insert copy [] make error! ""
)
(use [x] [x: 1 strict-equal? 1 redbol-apply func ['x] [:x] [:x]])
(use [x] [x: 1 strict-equal? 1 redbol-apply func ['x] [:x] [:x]])
(
    use [x] [
        x: 1
        strict-equal? first [:x] redbol-apply/only func [:x] [:x] [:x]
    ]
)
(
    use [x] [
        unset 'x
        strict-equal? first [:x] redbol-apply/only func ['x [<opt> any-value!]] [
            return get 'x
        ] [:x]
    ]
)
(use [x] [x: 1 strict-equal? 1 redbol-apply func [:x] [:x] [x]])
(use [x] [x: 1 strict-equal? 'x redbol-apply func [:x] [:x] ['x]])
(use [x] [x: 1 strict-equal? 'x redbol-apply/only func [:x] [:x] [x]])
(use [x] [x: 1 strict-equal? 'x redbol-apply/only func [:x] [return :x] [x]])
(
    use [x] [
        unset 'x
        strict-equal? 'x redbol-apply/only func ['x [<opt> any-value!]] [
            return get 'x
        ] [x]
    ]
)

; MAKE FRAME! :RETURN should preserve binding in the FUNCTION OF the frame
;
(1 == eval func [] [redbol-apply :return [1] 2])

(_ == redbol-apply/only func [/a] [a] [#[false]])
(group! == redbol-apply/only :type-of [()])