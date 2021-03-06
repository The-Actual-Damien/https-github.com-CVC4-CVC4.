; COMMAND-LINE: --unconstrained-simp --no-check-models
(set-logic QF_AUFBVLIA)
(set-info :smt-lib-version 2.6)
(set-info :category "crafted")
(set-info :status sat)
(declare-fun x0 () (_ BitVec 10))
(declare-fun x1 () (_ BitVec 10))
(declare-fun x2 () (_ BitVec 10))
(declare-fun x3 () (_ BitVec 10))
(declare-fun x4 () (_ BitVec 10))
(declare-fun x5 () (_ BitVec 10))
(declare-fun x6 () (_ BitVec 10))
(declare-fun x7 () (_ BitVec 10))
(declare-fun v2 () Int)
(declare-fun a2 (Int) (_ BitVec 1024))
(declare-fun v3 () (_ BitVec 1024))
(declare-fun v4 () (_ BitVec 1024))
(declare-fun v5 () (_ BitVec 1024))
(assert
 (not
  (=
   (a2
    (ite 
     (= (xor (= (bvxor (bvxnor (bvadd (bvsub x0 ((_ extract 9 0) v3)) ((_ extract
     9 0) v4)) ((_ extract 9 0) v5)) ((_ extract 19 10) v3)) ((_ extract 19 10)
     v4)) (= v3 v4)) (= v4 v5))
     v2
     6)
    )
   (bvudiv (bvudiv v4 v5) (bvudiv (bvudiv v3 v4) (bvudiv v3 v5))))
  )
 )
(check-sat)
(exit)
