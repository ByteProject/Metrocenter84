! ----------------------------------------------------------------------------
!  Metrocenter '84 version of SCENERY
!  (c) Fredrik Ramsberg (author) 2020.
!  (c) Stefan Vogt, Puddle Software 2020.
! ----------------------------------------------------------------------------

System_file;

Object GenericScenery "scenery"
    with
        article "a",
        number 0,
        parse_name [ w1 w2 i sw1 sw2 len;
            w1 = NextWordStopped();
            w2 = NextWordStopped();
            i = 0;
            len = location.#metro_scenery / 2;
            #ifdef DEBUG;
            if(len % 3 > 0)
                "ERROR: metro_scenery property of current location seems to hold an incorrect number of values!^";
            while(i < len) {
                sw1 = location.&metro_scenery-->(i+2);
                if(~~(sw1 ofclass String or Routine))
                    "ERROR: Element ",i+2," in metro_scenery property of current location is not a string or routine!^",
                        "Element: ", (name) sw1, "^";
                i = i + 3;
            }
            i = 0;
            #endif;
            while(i < len) {
                sw1 = location.&metro_scenery-->i;
                sw2 = location.&metro_scenery-->(i+1);
                if(w1 == sw1 && w2 == sw2) {
                    self.number = i;
                    return 2;
                }
                if(w1 == sw1 or sw2) {
                    self.number = i;
                    return 1;
                }
                i = i + 3;
            }
            ! It would make sense to return 0 here, but property
            ! routines return 0 by default anyway.
        ],
        description [k;
            k = location.&metro_scenery-->(self.number + 2);
            if(k ofclass Routine) {
                k();
                rtrue;
            }
            print_ret (string) k;
        ],
        before [;
            Examine:
                rfalse;
            default:
                #ifdef SceneryReply;
                if(SceneryReply ofclass string)
                    print_ret (string) SceneryReply;
                if(SceneryReply())
                    rtrue;
                #endif;
                "No need to concern yourself with that.";
        ],
    has concealed scenery;

[InScope;
    if(scope_reason == PARSING_REASON && location provides metro_scenery)
        PlaceInScope(GenericScenery);
    rfalse;
];
