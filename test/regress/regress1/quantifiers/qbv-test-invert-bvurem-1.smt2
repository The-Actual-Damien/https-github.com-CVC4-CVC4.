; COMMAND-LINE: --cegqi-bv --cegqi-bv-ineq=keep --no-cegqi-full -q
; EXPECT: sat
(set-logic BV)
(set-info :status sat)
(declare-fun a () (_ BitVec 8))
(declare-fun b () (_ BitVec 8))

(assert (forall ((x (_ BitVec 8))) (not (= (bvurem a x) b))))

(check-sat)
