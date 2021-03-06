; datatypes/unset.r
(null? null)
(null? type of null)
(not null? 1)

(
    is-barrier?: func [x [<end> integer!]] [unset? 'x]
    is-barrier? ()
)
(void! = type of (do []))
(not void? 1)

[#68 https://github.com/metaeducation/ren-c/issues/876
    ('need-non-end = (trap [a: ()])/id)
]

(error? trap [set* lit a: null a])
(not error? trap [set* 'a null])

(error? trap [set* lit a: void a])
(not error? trap [set* 'a void])

(
    a-value: 10
    unset 'a-value
    e: trap [a-value]
    e/id = 'no-value
)
