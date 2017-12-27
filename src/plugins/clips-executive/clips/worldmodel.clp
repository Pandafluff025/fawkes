
;---------------------------------------------------------------------------
;  worldmodel.clp - CLIPS executive - generic world model representation
;
;  Created: Wed Oct 18 11:40:06 2017
;  Copyright  2017  Tim Niemueller [www.niemueller.de]
;  Licensed under GPLv2+ license, cf. LICENSE file
;---------------------------------------------------------------------------

; # World Model Facts
; Facts represent information in the world model. They are identified by a
; unique ID in the world model. This ID is used, for example, to reference
; and element during, e.g., storage and communication.
; However, for a more natural representation in CLIPS and with other
; reasoning mechanisms, we keep a dual representation called key, which is
; a multifield representation of the ID. They are intended to match
; one-to-one.
; Note that we need to make the representation compatible with the domain
; model, e.g., with predicates specified in PDDL. However, we want to
; provide a richer representation to cover more CLIPS and other reasoner's
; use-cases. Therefore, our identification scheme allows to represent a
; superset of these simpler representations.
;
; ## IDs
; The ID format is inspired by URLs, allowing for an easy matching
; later to a RESTful interface or to key-value (or more expressive)
; storage or communication mechanisms.
;
; General format: /prefix/path/value?arg1=val1&arg2=val2...
;
; Basic names may contain letters, numbers, dashes, or
; underscores. They may in particular *not* contain question marks,
; ampersands, equal signs, slashes, or commas.
;
; At the very minimum, an element has a filesystem path like name,
; that is basic names joined with a slash (/) as separator.  Note that
; typically, the first prefix of a path identifies the origin or
; authoritative source for information, well-known names, for
; examples, could be config or domain.
;
; An ID may have an *optional* argument string, separated from the
; path by a question mark (?). It allows for a natural representation
; of named arguments as part of the ID. This is, for example,
; necessary to represent predicates in this format.  The argument
; string is a list of pairs of a basic name and a value. Values can be
; basic names, numbers, or lists. Lists are concatenate the elements
; separated by a comma, e.g., "fruits=apple,orange". Multiple
; arguments can be added separated by an ampersand (&).
;
; Examples:
; - /wm/pose
; - /wm/holding
; - /config/clips-exec/clips-debug
; - /domain/fact/loaded?robot=R-1&objects=foo,bar
; - /domain/fact/loaded?objects=foo2,bar2&robot=R-2
; - /domain/fact/holding?object=some-obj&robot=R-2
;
; ## Keys

; Keys are a dual representation to IDs. They encode the exact same
; information, but in a format more natural to be used within CLIPS.
; Please read about the principal format of IDs to understand the
; elements in keys.
;
; General format: (key prefix path value args? arg1 val1 arg2 val2)
;
; The general format is a multifield encoding the separate elements of
; the ID. The path is the list of elements in the ID separated by
; slashes. The optional argument string is separated by the symbol
; args?. The remaining list must be of alternating name value pairs.
; To encode a list value, the values are split into their separate
; values and enclosed by opening and closing square bracket symbols,
; e.g., "fruits [ apple orange ]". Please note that the spaces around
; the brackets are mandatory in order to identify them as separate
; symbols!
;
; Examples:
; - (key wm pose)
; - (key wm holding)
; - (key config clips-exec clips-debug)
; - (key domain fact loaded args? robot R-1 objects [ foo bar ])
; - (key domain fact loaded args? objects [ foo2 bar2 ] robot R-2)
; - (key domain fact holding args? object some-obj robot R-2)
;
; ## Creating Fact IDs and Keys
; When creating a fact, you can specify either representation, an ID or
; a key. The system will take care of automatically generating the
; other representation.
;
; In general, keys and IDs should never be modified! In that case,
; explicitly retract the old fact and assert a new one with only the
; updated ID or key. Note that the system would _not_ automaticaly
; update the dual representation if you were to modify either of ID or
; key.

(deftemplate wm-fact
	; ID is a path-like string, there is a direct matching from ID to key
	; established by the function wm-id-to-key.
	(slot id (type STRING))
	(multislot key (type SYMBOL))
	; The type definition is optional and informative, it is used in
	; particular in combination with config values
  (slot type (type SYMBOL) (allowed-values UNKNOWN FLOAT UINT INT BOOL STRING))
	(slot is-list (type SYMBOL) (allowed-values FALSE TRUE))
	; Use either slot depending on whether it's a single value or a list
	(slot value)
	(multislot values)
)

(deffunction wm-split-string-sym (?str ?sep)
	"Split a string into symbols.
   @param ?str string to split
   @param ?sep separator of string
   @return list of symbols of elements in ?str split by ?sep
  "
	(bind ?rv (create$))
	(bind ?idx (str-index ?sep ?str))
	(while ?idx do
		(bind ?e (sub-string 1 (- ?idx 1) ?str))
		(if (> (str-length ?e) 0) then (bind ?rv (append$ ?rv (sym-cat ?e))))
		(bind ?str (sub-string (+ ?idx 1) (str-length ?str) ?str))
		(bind ?idx (str-index ?sep ?str))
	)
	(if (> (str-length ?str) 0) then (bind ?rv (append$ ?rv (sym-cat ?str))))
	(return ?rv)
)

(deffunction wm-id-to-key (?id)
	"Convert an ID to a key (multifield).
   @param ?id ID to convert
   @return key splitted key
  "
	(bind ?rv (create$))

	(bind ?idx (str-index "?" ?id))
	(if ?idx
	 then
		(bind ?path   (sub-string 1 (- ?idx 1) ?id))
		(bind ?params (sub-string (+ ?idx 1) (str-length ?id) ?id))
		(bind ?rv (wm-split-string-sym ?path "/"))
		(bind ?rv (append$ ?rv args?))
		(bind ?ps (wm-split-string-sym ?params "&"))
		(foreach ?p ?ps
			(bind ?kv (wm-split-string-sym ?p "="))
			(if (<> (length$ ?kv) 2) then
				(printout w "Param '" ?p "' not of format name=value, ignoring" crlf)
				(bind ?rv (append$ ?rv INVALID (sym-cat ?p)))
			 else
				(if (str-index "," (nth$ 2 ?kv)) then
				 then
					(bind ?vargs (wm-split-string-sym (nth$ 2 ?kv) ","))
					(bind ?rv (append$ ?rv (sym-cat (nth$ 1 ?kv)) [))
					(loop-for-count (?i (length$ ?vargs)) (bind ?rv (append$ ?rv (sym-cat (nth$ ?i ?vargs)))))
					(bind ?rv (append$ ?rv ]))
				 else
					(bind ?rv (append$ ?rv (sym-cat (nth$ 1 ?kv)) (sym-cat (nth$ 2 ?kv))))
				)
			)
		)
	else	
		(bind ?rv (wm-split-string-sym ?id "/"))
	)

	(return ?rv)			 
)

(deffunction wm-key-to-id ($?key)
	(bind ?rv "")

	(bind ?L (length$ ?key))
	(bind ?l 1)
	(bind ?proc-args FALSE)

	(while (<= ?l ?L)
		(bind ?ke (nth$ ?l ?key))
		(if (eq ?ke args?)
		 then
			(bind ?rv (str-cat ?rv "?"))
			(bind ?proc-args TRUE)
			(bind ?l (+ ?l 1))
			(break)
		 else
			(bind ?rv (str-cat ?rv "/" ?ke))
		)
		(bind ?l (+ ?l 1))
	)

	(if ?proc-args then
		(while (<= (+ ?l 1) ?L) do
			(bind ?arg (nth$ ?l ?key))
			(bind ?value (nth$ (+ ?l 1) ?key))
			(if (and (eq ?value [) (<= (+ ?l 2) ?L)) then
				; it's a list, process until end
				(bind ?ao 2) ; args offset: offset from ?l of next and eventually last arg
				(while (<= (+ ?l ?ao) ?L) do
					(bind ?va (nth$ (+ ?l ?ao) ?key))
					(if (or (eq ?va ]) (= (+ ?l ?ao) ?L))
					 ; we hit the closing bracket or end of the arguments
					 then
						; conclude list
						(bind ?rv (str-cat ?rv ?arg "=" (nth$ (+ ?l 2) ?key)))
						(bind ?ai 3) ; args index from [?l .. ?l + ?ao]
						; arg index end, it's ?l+?ao, unless we have ']', then remove that
						(bind ?ai-end (+ ?l ?ao (if (eq ?va ]) then -1 else 0)))
						(while (<= (+ ?l ?ai) ?ai-end) do
							(bind ?rv (str-cat ?rv "," (nth$ (+ ?l ?ai) ?key)))
							(bind ?ai (+ ?ai 1))
						)
						(if (< (+ ?l ?ao) ?L) then (bind ?rv (str-cat ?rv "&")))
						(break)
					 else
						(bind ?ao (+ ?ao 1))
					)
				)
				(bind ?l (+ ?l ?ao 1))
			 else
				(bind ?rv (str-cat ?rv ?arg "=" (nth$ (+ ?l 1) ?key) (if (< (+ ?l 2) ?L) then "&" else "")))
				(bind ?l (+ ?l 2))
			)
		)
		(if (<= ?l ?L) then
			(printout warn "Missing value for argument " (nth$ ?l ?key) " in " ?key crlf)
		)
	)

	(return ?rv)
)

(defrule wm-fact-id2key
	"If a world model fact is added with an ID but no key"
	(declare (salience 700))
	?w <- (wm-fact (id ?id) (key))
	=>
	(modify ?w (key (wm-id-to-key ?id)))
)

(defrule wm-fact-key2id
	"If a world model fact is added with a key but no ID"
	(declare (salience 700))
	?w <- (wm-fact (id "") (key $?key))
	=>
	(modify ?w (id (wm-key-to-id ?key)))
)

(defrule wm-key-conflict
	(declare (salience 500))
	?w1 <- (wm-fact (id ?id1) (key $?key))
	?w2 <- (wm-fact (id ?id2) (key $?key))
	(test (neq ?w1 ?w2))
	=>
	(printout error "Two world model facts with the same key '" ?key '": "
						?w1 " (" ?id1 ") and " ?w2 " (" ?id2 ")" crlf)
	(printout error "Fact " ?w1 ":" crlf)
	(ppfact ?w1 error TRUE)
	(printout error "Fact " ?w2 ":" crlf)
	(ppfact ?w2 error TRUE)
	(printout error "Erasing " ?w2 crlf)
	(retract ?w2)
)
