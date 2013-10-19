/*++
Copyright (c) 2011 Microsoft Corporation

Module Name:

    iz3proof.cpp

Abstract:

   This class defines a simple interpolating proof system.

Author:

    Ken McMillan (kenmcmil)

Revision History:

--*/

#include "iz3proof_itp.h"

#ifndef WIN32
using namespace stl_ext;
#endif


class iz3proof_itp_impl : public iz3proof_itp {

  prover *pv;
  prover::range rng;
  bool weak;

  enum LitType {LitA,LitB,LitMixed};

  hash_map<ast,ast> placeholders;

  // These symbols represent deduction rules

  /* This symbol represents a proof by contradiction. That is,
     contra(p,l1 /\ ... /\ lk) takes a proof p of

     l1,...,lk |- false

     and returns a proof of 

     |- ~l1,...,~l2
  */
  symb contra;

  /* The summation rule. The term sum(p,c,i) takes a proof p of an
     inequality i', an integer coefficient c and an inequality i, and
     yieds a proof of i' + ci. */
  symb sum;

  /* Proof rotation. The proof term rotate(q,p) takes a
     proof p of:

     Gamma, q |- false

     and yields a proof of:

     Gamma |- ~q
  */
  symb rotate_sum;

  /* Inequalities to equality. leq2eq(p, q, r) takes a proof
     p of ~x=y, a proof q of x <= y and a proof r of y <= x
     and yields a proof of false. */
  symb leq2eq;

  /* Equality to inequality. eq2leq(p, q) takes a proof p of x=y, and
     a proof q ~(x <= y) and and yields a proof of false. */
  symb eq2leq;

  /* Proof term cong(p,q) takes a proof p of x=y and a proof
     q of t != t<y/x> and returns a proof of false. */
  symb cong;


  /* Excluded middle. exmid(phi,p,q) takes a proof p of phi and a
     proof q of ~\phi and returns a proof of false. */
  symb exmid;

  /* Symmetry. symm(p) takes a proof p of x=y and produces
     a proof of y=x. */
  symb symm;

  /* Modus ponens. modpon(p,e,q) takes proofs p of P, e of P=Q
     and q of ~Q and returns a proof of false. */
  symb modpon;

  /* This represents a lack of a proof */
  ast no_proof;

  // This is used to represent an infinitessimal value
  ast epsilon;

  ast get_placeholder(ast t){
    hash_map<ast,ast>::iterator it = placeholders.find(t);
    if(it != placeholders.end())
      return it->second;
    ast &res = placeholders[t];
    res = mk_fresh_constant("@p",get_type(t));
    std::cout << "placeholder ";
    print_expr(std::cout,res);
    std::cout << " = ";
    print_expr(std::cout,t);
    std::cout << std::endl;
    return res;
  }

  ast make_contra_node(const ast &pf, const std::vector<ast> &lits, int pfok = -1){
    if(lits.size() == 0)
      return pf;
    std::vector<ast> reslits;
    reslits.push_back(make(contra,pf,mk_false()));
    for(unsigned i = 0; i < lits.size(); i++){
      ast bar;
      if(pfok & (1 << i)) bar = make(rotate_sum,lits[i],pf);
      else bar = no_proof;
      ast foo = make(contra,bar,lits[i]);
      reslits.push_back(foo);
    }
    return make(And,reslits);
  }

  LitType get_term_type(const ast &lit){
    prover::range r = pv->ast_scope(lit);
    if(pv->range_is_empty(r))
      return LitMixed;
    if(weak) {
      if(pv->range_min(r) == SHRT_MIN)
	return pv->range_contained(r,rng) ? LitA : LitB;
      else
	return pv->ranges_intersect(r,rng) ? LitA : LitB;
    }
    else
      return pv->range_contained(r,rng) ? LitA : LitB;
  }

  bool term_common(const ast &t){
    prover::range r = pv->ast_scope(t);
    return pv->ranges_intersect(r,rng) && !pv->range_contained(r,rng);
  }

  /** Make a resolution node with given pivot literal and premises.
      The conclusion of premise1 should contain the negation of the
      pivot literal, while the conclusion of premise2 should contain the
      pivot literal.
   */
  node make_resolution(ast pivot, const std::vector<ast> &conc, node premise1, node premise2) {
    LitType lt = get_term_type(pivot);
    if(lt == LitA)
      return my_or(premise1,premise2);
    if(lt == LitB)
      return my_and(premise1,premise2);
    
    /* the mixed case is a bit complicated */

    return resolve_arith(pivot,conc,premise1,premise2);
  }

#if 0
  /* Handles the case of resolution on a mixed equality atom.  */

  ast resolve_equality(const ast &pivot, const std::vector<ast> &conc, node premise1, node premise2){
    ast atom = get_lit_atom(pivot);
    
    /* Get the A term in the mixed equality. */
    ast A_term = arg(atom,0);
    if(get_term_type(A_term) != LitA)
      A_term = arg(atom,1);

    /* Resolve it out. */
    hash_map<ast,ast> memo;
    // ast neg_pivot_placeholder = get_placeholder(mk_not(atom));
    ast neg_pivot_placeholder = mk_not(atom);
    ast A_term_placeholder = get_placeholder(atom);
    if(op(pivot) != Not)
      std::swap(premise1,premise2);
    return resolve_equality_rec(memo, neg_pivot_placeholder, premise1, A_term_placeholder, premise2);
  }
  
  ast resolve_equality_rec(hash_map<ast,ast> &memo, const ast &neg_pivot_placeholder, const ast &premise1, 
					  const ast &A_term, const ast &premise2){
    ast &res = memo[premise1];
    if(!res.null())
      return res;
    
    if(op(premise1) == And
       && num_args(premise1) == 2
       && arg(premise1,1) == neg_pivot_placeholder){
      ast common_term = arg(arg(premise1,0),1);
      res = subst_term_and_simp(A_term,common_term,premise2);
    }
    else {
      switch(op(premise1)){
      case Or:
      case And: 
      case Implies: {
	unsigned nargs = num_args(premise1);
	std::vector<ast> args; args.resize(nargs);
	for(unsigned i = 0; i < nargs; i++)
	  args[i] = resolve_equality_rec(memo, neg_pivot_placeholder, arg(premise1,i), A_term, premise2);
	ast foo = premise1; // get rid of const
	res = clone(foo,args);
	break;
      }
      default:
	res = premise1;
      }
    }
    return res;
  }
#endif

  /* Handles the case of resolution on a mixed arith atom.  */

  ast resolve_arith(const ast &pivot, const std::vector<ast> &conc, node premise1, node premise2){
    ast atom = get_lit_atom(pivot);
    hash_map<ast,ast> memo;
    ast neg_pivot_lit = mk_not(atom);
    if(op(pivot) != Not)
      std::swap(premise1,premise2);
    return resolve_arith_rec1(memo, neg_pivot_lit, premise1, premise2);
  }
  

  ast apply_coeff(const ast &coeff, const ast &t){
#if 0
    rational r;
    if(!is_integer(coeff,r))
      throw "ack!";
    ast n = make_int(r.numerator());
    ast res = make(Times,n,t);
    if(!r.is_int()) {
      ast d = make_int(r.numerator());
      res = mk_idiv(res,d);
    }
    return res;
#endif
    return make(Times,coeff,t);
  }

  ast sum_ineq(const ast &coeff1, const ast &ineq1, const ast &coeff2, const ast &ineq2){
    opr sum_op = Leq;
    if(op(ineq1) == Lt || op(ineq2) == Lt)
      sum_op = Lt;
    ast sum_sides[2];
    for(int i = 0; i < 2; i++){
      sum_sides[i] = make(Plus,apply_coeff(coeff1,arg(ineq1,i)),apply_coeff(coeff2,arg(ineq2,i)));
      sum_sides[i] = z3_simplify(sum_sides[i]);
    }
    return make(sum_op,sum_sides[0],sum_sides[1]);
  }
  
#if 0
  ast resolve_farkas(const ast &pivot1, const ast &conj1, const ast &implicant1,
		     const ast &pivot2, const ast &conj2, const ast &implicant2){
    std::vector<ast> resolvent;
    ast coeff1 = get_farkas_coeff(pivot1);
    ast coeff2 = get_farkas_coeff(pivot2);
    ast s1 = resolve_arith_placeholders(pivot2,conj2,arg(conj1,0));
    ast s2 = resolve_arith_placeholders(pivot1,conj1,arg(conj2,0));
    resolvent.push_back(sum_ineq(coeff2,s1,coeff1,s2));
    collect_farkas_resolvents(pivot1,coeff2,conj1,resolvent);
    collect_farkas_resolvents(pivot2,coeff1,conj2,resolvent);
    ast res = make(And,resolvent);
    if(implicant1.null() && implicant2.null())
      return res;
    ast i1 = implicant1.null() ? mk_false() : resolve_arith_placeholders(pivot2,conj2,implicant1);
    ast i2 = implicant2.null() ? mk_false() : resolve_arith_placeholders(pivot1,conj1,implicant2);
    return make(Implies,res,my_or(i1,i2));
  }
#endif

  void collect_contra_resolvents(int from, const ast &pivot1, const ast &pivot, const ast &conj, std::vector<ast> &res){
    int nargs = num_args(conj);
    for(int i = from; i < nargs; i++){
      ast f = arg(conj,i);
      if(!(f == pivot)){
	ast ph = get_placeholder(mk_not(arg(pivot1,1)));
	ast pf = arg(pivot1,0);
	ast thing = pf == no_proof ? no_proof : subst_term_and_simp(ph,pf,arg(f,0));
	ast newf = make(contra,thing,arg(f,1));
	res.push_back(newf);
      }
    }
  }
  
  bool is_negative_equality(const ast &e){
    if(op(e) == Not){
      opr o = op(arg(e,0));
      return o == Equal || o == Iff;
    }
    return false;
  }

  int count_negative_equalities(const std::vector<ast> &resolvent){
    int res = 0;
    for(unsigned i = 0; i < resolvent.size(); i++)
      if(is_negative_equality(arg(resolvent[i],1)))
	res++;
    return res;
  }

  ast resolve_contra_nf(const ast &pivot1, const ast &conj1,
		     const ast &pivot2, const ast &conj2){
    std::vector<ast> resolvent;
    collect_contra_resolvents(0,pivot1,pivot2,conj2,resolvent);
    collect_contra_resolvents(1,pivot2,pivot1,conj1,resolvent);
    if(count_negative_equalities(resolvent) > 1)
      throw proof_error();
    if(resolvent.size() == 1) // we have proved a contradiction
      return simplify(arg(resolvent[0],0)); // this is the proof -- get interpolant
    return make(And,resolvent);
  }

  ast resolve_contra(const ast &pivot1, const ast &conj1,
			  const ast &pivot2, const ast &conj2){
#if 0
    int nargs = num_args(conj1);
    for(int i = 0; i < nargs; i++){
      ast foo = arg(conj1,i);
      if(!(foo == pivot1) && is_negative_equality(arg(foo,1)))
	return resolve_contra_nf(pivot2, conj2, pivot1, conj1);
    }
#endif
    if(arg(pivot1,0) != no_proof)
      return resolve_contra_nf(pivot1, conj1, pivot2, conj2);
    if(arg(pivot2,0) != no_proof)
      return resolve_contra_nf(pivot2, conj2, pivot1, conj1);
    return resolve_with_quantifier(pivot1, conj1, pivot2, conj2);
  }


  bool is_contra_itp(const ast &pivot1, ast itp2, ast &pivot2){
    if(op(itp2) == And){
      int nargs = num_args(itp2);
      for(int i = 1; i < nargs; i++){
	ast foo = arg(itp2,i);
	if(op(foo) == Uninterpreted && sym(foo) == contra){
	  if(arg(foo,1) == pivot1){
	    pivot2 = foo;
	    return true;
	  }
	}
	else break;
      }
    }
    return false;
  }

  ast resolve_arith_rec2(hash_map<ast,ast> &memo, const ast &pivot1, const ast &conj1, const ast &itp2){
    ast &res = memo[itp2];
    if(!res.null())
      return res;
    
    ast pivot2;
    if(is_contra_itp(mk_not(arg(pivot1,1)),itp2,pivot2))
      res = resolve_contra(pivot1,conj1,pivot2,itp2);
    else {
      switch(op(itp2)){
      case Or:
      case And:
      case Implies: {
	unsigned nargs = num_args(itp2);
	std::vector<ast> args; args.resize(nargs);
	for(unsigned i = 0; i < nargs; i++)
	  args[i] = resolve_arith_rec2(memo, pivot1, conj1, arg(itp2,i));
	ast foo = itp2; // get rid of const
	res = clone(foo,args);
	break;
      }
      default:
	res = itp2;
      }
    }
    return res;
  }

  ast resolve_arith_rec1(hash_map<ast,ast> &memo, const ast &neg_pivot_lit, const ast &itp1, const ast &itp2){
    ast &res = memo[itp1];
    if(!res.null())
      return res;
    ast pivot1;
    if(is_contra_itp(neg_pivot_lit,itp1,pivot1)){
      hash_map<ast,ast> memo2;
      res = resolve_arith_rec2(memo2,pivot1,itp1,itp2);
    }
    else {
      switch(op(itp1)){
      case Or:
      case And:
      case Implies: {
	unsigned nargs = num_args(itp1);
	std::vector<ast> args; args.resize(nargs);
	for(unsigned i = 0; i < nargs; i++)
	  args[i] = resolve_arith_rec1(memo, neg_pivot_lit, arg(itp1,i), itp2);
	ast foo = itp1; // get rid of const
	res = clone(foo,args);
	break;
      }
      default:
	res = itp1;
      }
    }
    return res;
  }

#if 0
  ast resolve_arith_placeholders(const ast &pivot1, const ast &conj1, const ast &itp2){
    ast coeff = arg(pivot1,0);
    ast val = arg(arg(conj1,0),1);
    if(op(conj1)==Lt)
      val = make(Sub,val,epsilon);  // represent x < c by x <= c - epsilon
    coeff = z3_simplify(coeff);
    ast soln = mk_idiv(val,coeff);
    int nargs = num_args(conj1);
    for(int i = 1; i < nargs; i++){
      ast c = arg(conj1,i);
      if(!(c == pivot1)){
	soln = make(Plus,soln,get_placeholder(arg(c,1)));
      }
    }
    ast pl = get_placeholder(mk_not(arg(pivot1,1)));
    ast res = subst_term_and_simp(pl,soln,itp2);
    return res;
  }
#endif

  hash_map<ast,ast> subst_memo;                    // memo of subst function

  ast subst_term_and_simp(const ast &var, const ast &t, const ast &e){
    subst_memo.clear();
    return subst_term_and_simp_rec(var,t,e);
  }

  ast subst_term_and_simp_rec(const ast &var, const ast &t, const ast &e){
    if(e == var) return t;
    std::pair<ast,ast> foo(e,ast());
    std::pair<hash_map<ast,ast>::iterator,bool> bar = subst_memo.insert(foo);
    ast &res = bar.first->second;
    if(bar.second){
      int nargs = num_args(e);
      std::vector<ast> args(nargs);
      for(int i = 0; i < nargs; i++)
	args[i] = subst_term_and_simp_rec(var,t,arg(e,i));
      opr f = op(e);
      if(f == Equal && args[0] == args[1]) res = mk_true();
      else if(f == And) res = my_and(args);
      else if(f == Or) res = my_or(args);
      else if(f == Idiv) res = mk_idiv(args[0],args[1]);
      else res = clone(e,args);
    }
    return res;
  }

  /* This is where the real work happens. Here, we simplify the
     proof obtained by cut elimination, obtaining an interpolant. */

  struct cannot_simplify {};
  hash_map<ast,ast> simplify_memo;

  ast simplify(const ast &t){
    return simplify_rec(t);
  }

  ast simplify_rec(const ast &e){
    std::pair<ast,ast> foo(e,ast());
    std::pair<hash_map<ast,ast>::iterator,bool> bar = simplify_memo.insert(foo);
    ast &res = bar.first->second;
    if(bar.second){
      int nargs = num_args(e);
      std::vector<ast> args(nargs);
      bool placeholder_arg = false;
      for(int i = 0; i < nargs; i++){
	args[i] = simplify_rec(arg(e,i));
	placeholder_arg |= is_placeholder(args[i]);
      }
      try {
	opr f = op(e);
	if(f == Equal && args[0] == args[1]) res = mk_true();
	else if(f == And) res = my_and(args);
	else if(f == Or) res = my_or(args);
	else if(f == Idiv) res = mk_idiv(args[0],args[1]);
	else if(f == Uninterpreted && !placeholder_arg){
	  symb g = sym(e);
	  if(g == rotate_sum) res = simplify_rotate(args);
	  else if(g == symm) res = simplify_symm(args);
	  else if(g == modpon) res = simplify_modpon(args);
	  else if(g == sum) res = simplify_sum(args);
#if 0
	  else if(g == cong) res = simplify_cong(args);
	  else if(g == modpon) res = simplify_modpon(args);
	  else if(g == leq2eq) res = simplify_leq2eq(args);
	  else if(g == eq2leq) res = simplify_eq2leq(args);
#endif
	  else res = clone(e,args);
	}
	else res = clone(e,args);
      }
      catch (const cannot_simplify &){
	res = clone(e,args);
      }
    }
    return res;
  }


  ast simplify_rotate(const std::vector<ast> &args){
    const ast &pf = args[1];
    ast pl = get_placeholder(args[0]);
    if(op(pf) == Uninterpreted){
      symb g = sym(pf);
      if(g == sum) return simplify_rotate_sum(pl,pf);
      if(g == leq2eq) return simplify_rotate_leq2eq(pl,args[0],pf);
      if(g == eq2leq) return simplify_rotate_eq2leq(pl,args[0],pf);
      if(g == cong) return simplify_rotate_cong(pl,args[0],pf);
      if(g == modpon) return simplify_rotate_modpon(pl,args[0],pf);
      // if(g == symm) return simplify_rotate_symm(pl,args[0],pf);
    }
    throw cannot_simplify();
  }

  ast simplify_sum(std::vector<ast> &args){
    ast cond = mk_true();
    ast ineq = args[0];
    if(!is_ineq(ineq)) throw cannot_simplify();
    sum_cond_ineq(ineq,cond,args[1],args[2]);
    return my_implies(cond,ineq);
  }

  ast simplify_rotate_sum(const ast &pl, const ast &pf){
    ast cond = mk_true();
    ast ineq = make(Leq,make_int("0"),make_int("0"));
    return rotate_sum_rec(pl,pf,cond,ineq);
  }

  void sum_cond_ineq(ast &ineq, ast &cond, const ast &coeff2, const ast &ineq2){
    opr o = op(ineq2);
    if(o == Implies){
      sum_cond_ineq(ineq,cond,coeff2,arg(ineq2,1));
      cond = my_and(cond,arg(ineq2,0));
    }
    else if(is_ineq(ineq2))
      linear_comb(ineq,coeff2,ineq2);
    else
      throw cannot_simplify();
  }
  
  bool is_ineq(const ast &ineq){
    opr o = op(ineq);
    if(o == Not) o = op(arg(ineq,0));
    return o == Leq || o == Lt || o == Geq || o == Gt;
  }

  // divide both sides of inequality by a non-negative integer divisor
  ast idiv_ineq(const ast &ineq, const ast &divisor){
    return make(op(ineq),mk_idiv(arg(ineq,0),divisor),mk_idiv(arg(ineq,1),divisor));
  }
  
  ast rotate_sum_rec(const ast &pl, const ast &pf, ast &cond, ast &ineq){
    if(pf == pl)
      return my_implies(cond,simplify_ineq(ineq));
    if(op(pf) == Uninterpreted && sym(pf) == sum){
      if(arg(pf,2) == pl){
	sum_cond_ineq(ineq,cond,make_int("1"),arg(pf,0));
	ineq = idiv_ineq(ineq,arg(pf,1));
	return my_implies(cond,ineq);
      }
      sum_cond_ineq(ineq,cond,arg(pf,1),arg(pf,2));
      return rotate_sum_rec(pl,arg(pf,0),cond,ineq);
    }
    throw cannot_simplify();
  }

  ast simplify_rotate_leq2eq(const ast &pl, const ast &neg_equality, const ast &pf){
    if(pl == arg(pf,0)){
      ast equality = arg(neg_equality,0);
      ast x = arg(equality,0);
      ast y = arg(equality,1);
      ast xleqy = round_ineq(arg(pf,1));
      ast yleqx = round_ineq(arg(pf,2));
      ast itpeq;
      if(get_term_type(x) == LitA)
	itpeq = make(Equal,x,z3_simplify(make(Plus,x,get_ineq_rhs(xleqy))));
      else if(get_term_type(y) == LitA)
	itpeq = make(Equal,z3_simplify(make(Plus,y,get_ineq_rhs(yleqx))),y);
      else 
	throw cannot_simplify();
      ast cond = mk_true();
      ast ineq = make(Leq,make_int("0"),make_int("0"));
      sum_cond_ineq(ineq,cond,make_int("-1"),xleqy);
      sum_cond_ineq(ineq,cond,make_int("-1"),yleqx);
      cond = z3_simplify(my_and(cond,ineq));
      return my_implies(cond,itpeq);
    }
    throw cannot_simplify();
  }

  ast round_ineq(const ast &ineq){
    if(!is_ineq(ineq))
      throw cannot_simplify();
    ast res = simplify_ineq(ineq);
    if(op(res) == Lt)
      res = make(Leq,arg(res,0),make(Sub,arg(res,1),make_int("1")));
    return res;
  }

  ast simplify_rotate_eq2leq(const ast &pl, const ast &neg_equality, const ast &pf){
    if(pl == arg(pf,1)){
      ast cond = mk_true();
      ast equa = sep_cond(arg(pf,0),cond);
      if(op(equa) == Equal)
	return my_implies(cond,z3_simplify(make(Leq,make_int("0"),make(Sub,arg(equa,1),arg(equa,0)))));
      if(op(equa) == True)
	return my_implies(cond,z3_simplify(make(Leq,make_int("0"),make_int("0"))));
    }
    throw cannot_simplify();
  }

  ast simplify_rotate_modpon(const ast &pl, const ast &neg_equality, const ast &pf){
    if(pl == arg(pf,2)){
      std::vector<ast> args; args.resize(3);
      args[0] = arg(pf,0);
      args[1] = arg(pf,1);
      args[2] = mk_true();
      return simplify_modpon(args);
    }    
    throw cannot_simplify();
  }

  ast get_ineq_rhs(const ast &ineq2){
    opr o = op(ineq2);
    if(o == Implies)
      return get_ineq_rhs(arg(ineq2,1));
    else if(o == Leq || o == Lt)
      return arg(ineq2,1);
    throw cannot_simplify();
  }

  ast simplify_rotate_cong(const ast &pl, const ast &neg_equality, const ast &pf){
    if(pl == arg(pf,2)){
      if(op(arg(pf,0)) == True)
	return mk_true();
      rational pos;
      if(is_numeral(arg(pf,1),pos)){
	int ipos = pos.get_unsigned();
	ast cond = mk_true();
	ast equa = sep_cond(arg(pf,0),cond);
	if(op(equa) == Equal){
	  ast pe = mk_not(neg_equality);
	  ast lhs = subst_in_arg_pos(ipos,arg(equa,0),arg(pe,0));
	  ast rhs = subst_in_arg_pos(ipos,arg(equa,1),arg(pe,1));
	  ast res = make(Equal,lhs,rhs);
	  return my_implies(cond,res);
	}
      }
    }
    throw cannot_simplify();
  }

  ast simplify_symm(const std::vector<ast> &args){
    if(op(args[0]) == True)
      return mk_true();
    ast cond = mk_true();
    ast equa = sep_cond(args[0],cond);
    if(is_equivrel(equa))
      return my_implies(cond,make(op(equa),arg(equa,1),arg(equa,0)));
    throw cannot_simplify();
  }

  ast simplify_modpon(const std::vector<ast> &args){
    if(op(args[1]) == True){
      ast cond = mk_true();
      ast P = sep_cond(args[0],cond);
      ast notQ = sep_cond(args[2],cond);
      ast Q = mk_not(notQ);
      ast d = op(notQ) == True ? P : op(P) == True ? notQ : mk_not(delta(P,Q));
      return my_implies(cond,d);
    }
    else {
      ast cond = mk_true();
      ast P = sep_cond(args[0],cond);
      ast PeqQ = sep_cond(args[1],cond);
      ast Q2 = sep_cond(args[2],cond);
      ast P1 = arg(PeqQ,0);
      ast Q1 = arg(PeqQ,1);
      if(op(P) == True){
	if(is_equivrel(P1) && is_equivrel(Q1)){ // special case of transitivity
	  if(arg(P1,0) == arg(Q1,0))
	    if(op(args[2]) == True)
	      return my_implies(cond,make(op(P1),arg(P1,1),arg(Q1,1)));
	}
	if(op(args[2]) == True)
	  throw cannot_simplify(); // return my_implies(cond,make(rewrite(P1,Q1)));
	ast A_rewrites = mk_true();
	ast foo = commute_rewrites(P1,Q1,mk_not(Q2),A_rewrites);
	return my_implies(cond,my_implies(my_implies(A_rewrites,foo),mk_false()));
      }
      else if(op(args[2]) == True){
	ast P2 = apply_common_rewrites(P,P,P1,cond);
	ast A_rewrites = mk_true();
	if(is_equivrel(P2)){
	  try {
	    ast foo = commute_rewrites(arg(P2,0),arg(P2,1),arg(Q1,1),A_rewrites);
	    ast P3 = make(op(P1),arg(P1,0),foo);
	    if(P3 == P2)
	      return my_implies(cond,Q1);
	    // return my_implies(cond,make(rewrite,my_implies(A_rewrites,P3),Q1));
	  }
	  catch(const rewrites_failed &){
	    std::cout << "foo!\n";
	  }
	}
	ast Q2 = apply_all_rewrites(P2,P1,Q1);
	return my_implies(cond,Q2);
      }
    }
    throw cannot_simplify();
  }

  bool is_equivrel(const ast &p){
    opr o = op(p);
    return o == Equal || o == Iff;
  }
  
  struct rewrites_failed{};

  /* Suppose p in Lang(B) and A |- p -> q and B |- q -> r. Return a z in Lang(B) such that
     B |- p -> z and A |- z -> q. Collect any side conditions in "rules". */

  ast commute_rewrites(const ast &p, const ast &q, const ast &r, ast &rules){
    if(q == r)
      return p;
    if(p == q)
      return r;
    else {
      ast rew = make(Equal,q,r);
      if(get_term_type(rew) == LitB){
	apply_common_rewrites(p,p,q,rules); // A rewrites must be over comon vocab
	return r;
      }
    }
    if(sym(p) != sym(q) || sym(q) != sym(r))
      throw rewrites_failed();
    int nargs = num_args(p);
    if(nargs != num_args(q) || nargs != num_args(r))
      throw rewrites_failed();
    std::vector<ast> args; args.resize(nargs);
    for(int i = 0; i < nargs; i++)
      args[i] = commute_rewrites(arg(p,i),arg(q,i),arg(r,i),rules);
    return clone(p,args);
  }

  ast apply_common_rewrites(const ast &p, const ast &q, const ast &r, ast &rules){
    if(q == r)
      return p;
    ast rew = make(Equal,q,r);
    if(term_common(rew)){
      if(p != q)
	throw rewrites_failed();
      rules = my_and(rules,rew);
      return r;
    }
    if(sym(p) != sym(q) || sym(q) != sym(r))
      return p;
    int nargs = num_args(p);
    if(nargs != num_args(q) || nargs != num_args(r))
      return p;
    std::vector<ast> args; args.resize(nargs);
    for(int i = 0; i < nargs; i++)
      args[i] = apply_common_rewrites(arg(p,i),arg(q,i),arg(r,i),rules);
    return clone(p,args);
  }

  ast apply_all_rewrites(const ast &p, const ast &q, const ast &r){
    if(q == r)
      return p;
    if(p == q)
      return r;
    if(sym(p) != sym(q) || sym(q) != sym(r))
      throw rewrites_failed();
    int nargs = num_args(p);
    if(nargs != num_args(q) || nargs != num_args(r))
      throw rewrites_failed();
    std::vector<ast> args; args.resize(nargs);
    for(int i = 0; i < nargs; i++)
      args[i] = apply_all_rewrites(arg(p,i),arg(q,i),arg(r,i));
    return clone(p,args);
  }

  ast delta(const ast &x, const ast &y){
    if(op(x) != op(y) || (op(x) == Uninterpreted && sym(x) != sym(y)) || num_args(x) != num_args(y))
      return make(Equal,x,y);
    ast res = mk_true();
    int nargs = num_args(x);
    for(int i = 0; i < nargs; i++)
      res = my_and(res,delta(arg(x,i),arg(y,i)));
    return res;
  }

  ast sep_cond(const ast &t, ast &cond){
    if(op(t) == Implies){
      cond = my_and(cond,arg(t,0));
      return arg(t,1);
    }
    return t;
  }

  /** Make an assumption node. The given clause is assumed in the given frame. */
  virtual node make_assumption(int frame, const std::vector<ast> &assumption){
    if(pv->in_range(frame,rng)){
      std::vector<ast> itp_clause;
      for(unsigned i = 0; i < assumption.size(); i++)
	if(get_term_type(assumption[i]) != LitA)
	  itp_clause.push_back(assumption[i]);
      ast res = my_or(itp_clause);
      return res;
    }
    else {
      return mk_true();
    }
}

  ast triv_interp(const symb &rule, const std::vector<ast> &premises){
    std::vector<ast> ps; ps.resize(premises.size());
    std::vector<ast> conjs;
    int mask = 0;
    for(unsigned i = 0; i < ps.size(); i++){
      ast p = premises[i];
      switch(get_term_type(p)){
      case LitA:
	ps[i] = p;
	break;
      case LitB:
	ps[i] = mk_true();
	break;
      default:
	ps[i] = get_placeholder(p); // can only prove consequent!
	if(i == ps.size()-1)
	  mask |= (1 << conjs.size());
	conjs.push_back(p);
      }
    }
    ast ref = make(rule,ps);
    ast res = make_contra_node(ref,conjs,mask);
    return res;
  }

  ast triv_interp(const symb &rule, const ast &p0, const ast &p1, const ast &p2){
    std::vector<ast> ps; ps.resize(3);
    ps[0] = p0;
    ps[1] = p1;
    ps[2] = p2;
    return triv_interp(rule,ps);
  }
  
  /** Make a modus-ponens node. This takes derivations of |- x
      and |- x = y and produces |- y */
  
  virtual node make_mp(const ast &p_eq_q, const ast &prem1, const ast &prem2){
    
    /* Interpolate the axiom p, p=q -> q */
    ast p = arg(p_eq_q,0);
    ast q = arg(p_eq_q,1);
    ast itp;
    if(get_term_type(p_eq_q) == LitMixed){
      itp = triv_interp(modpon,p,p_eq_q,mk_not(q));
    }
    else {
      if(get_term_type(p) == LitA){
	if(get_term_type(q) == LitA)
	  itp = mk_false();
	else {
	  if(get_term_type(p_eq_q) == LitA)
	    itp = q;
	  else
	    throw proof_error();
	}
      }
      else {
	if(get_term_type(q) == LitA){
	  if(get_term_type(make(Equal,p,q)) == LitA)
	    itp = mk_not(p);
	  else
	    throw proof_error();
      }
      else
	itp = mk_true();
      }
    }
    
    /* Resolve it with the premises */
    std::vector<ast> conc; conc.push_back(q); conc.push_back(mk_not(make(Equal,p,q)));
    itp = make_resolution(p,conc,itp,prem1);
    conc.pop_back();
    itp = make_resolution(p_eq_q,conc,itp,prem2);
    return itp;
  }

  /** Make an axiom node. The conclusion must be an instance of an axiom. */
  virtual node make_axiom(const std::vector<ast> &conclusion){
    prover::range frng = pv->range_full();
    int nargs = conclusion.size();
    std::vector<ast> largs(nargs);
    std::vector<ast> eqs;
    std::vector<ast> pfs;
    
    for(int i = 0; i < nargs; i++){
      ast argpf;
      ast lit = conclusion[i];
      largs[i] = localize_term(lit,frng,argpf);
      frng = pv->range_glb(frng,pv->ast_scope(largs[i]));
      if(largs[i] != lit){
	eqs.push_back(make_equiv(largs[i],lit));
	pfs.push_back(argpf);
      }
    }
    
    int frame = pv->range_max(frng);
    ast itp = make_assumption(frame,largs);
    
    for(unsigned i = 0; i < eqs.size(); i++)
      itp = make_mp(eqs[i],itp,pfs[i]);
    return itp;
  }

  /** Make a Contra node. This rule takes a derivation of the form
     Gamma |- False and produces |- \/~Gamma. */

  virtual node make_contra(node prem, const std::vector<ast> &conclusion){
    return prem;
  }

  /** Make hypothesis. Creates a node of the form P |- P. */

  virtual node make_hypothesis(const ast &P){
    if(is_not(P))
      return make_hypothesis(arg(P,0));
    switch(get_term_type(P)){
    case LitA:
      return mk_false();
    case LitB:
      return mk_true();
    default: // mixed hypothesis
      switch(op(P)){
      case Geq: 
      case Leq: 
      case Gt: 
      case Lt: {
	ast zleqz = make(Leq,make_int("0"),make_int("0"));
	ast fark1 = make(sum,zleqz,make_int("1"),get_placeholder(P));
	ast fark2 = make(sum,fark1,make_int("1"),get_placeholder(mk_not(P)));
	ast res = make(And,make(contra,fark2,mk_false()),
		       make(contra,get_placeholder(mk_not(P)),P),
		       make(contra,get_placeholder(P),mk_not(P)));
	return res;
      }
      default: {
	ast em = make(exmid,P,get_placeholder(P),get_placeholder(mk_not(P)));
	ast res = make(And,make(contra,em,mk_false()),
		       make(contra,get_placeholder(mk_not(P)),P),
		       make(contra,get_placeholder(P),mk_not(P)));
	return res;
      }
      }
    }
  }

  /** Make a Reflexivity node. This rule produces |- x = x */
  
  virtual node make_reflexivity(ast con){
    throw proof_error();
}
  
  /** Make a Symmetry node. This takes a derivation of |- x = y and
      produces | y = x */

  virtual node make_symmetry(ast con, const ast &premcon, node prem){
    ast x = arg(con,0);
    ast y = arg(con,1);
    ast p = make(op(con),y,x);
    if(p == premcon)
      std::cout << "ok\n";
    if(get_term_type(con) != LitMixed)
      return prem; // symmetry shmymmetry...
#if 0      
    LitType xt = get_term_type(x);
    LitType yt = get_term_type(y);
    ast A_term;
    
    if(xt == LitA && yt == LitB)
      A_term = x;
    else if(yt == LitA && xt == LitB)
      A_term = y;
    else
    ast itp = make(And,make(Equal,A_term,get_placeholder(p)),mk_not(con));
#endif
    ast em = make(exmid,con,make(symm,get_placeholder(p)),get_placeholder(mk_not(con)));
    ast itp = make(And,make(contra,em,mk_false()),
		   make(contra,make(symm,get_placeholder(mk_not(con))),p),
		   make(contra,make(symm,get_placeholder(p)),mk_not(con)));
    
    std::vector<ast> conc; conc.push_back(con);
    itp = make_resolution(p,conc,itp,prem);
    return itp;
  }

  /** Make a transitivity node. This takes derivations of |- x = y
      and |- y = z produces | x = z */

  virtual node make_transitivity(const ast &x, const ast &y, const ast &z, node prem1, node prem2){

    /* Interpolate the axiom x=y,y=z,-> x=z */
    ast p = make(Equal,x,y);
    ast q = make(Equal,y,z);
    ast r = make(Equal,x,z);
    ast equiv = make(Iff,p,r);
    ast itp;

    itp = make_congruence(q,equiv,prem2);
    itp = make_mp(equiv,prem1,itp);

#if 0
    if(get_term_type(p) == LitA){
      if(get_term_type(q) == LitA){
	if(get_term_type(r) == LitA)
	  itp = mk_false();
	else 
	  itp = r;
      }
      else {
	if(get_term_type(r) == LitA)
	  itp = mk_not(q);
	else { 
	  if(get_term_type(r) == LitB)
	    itp = make(Equal,x,get_placeholder(q));
	  else {
	    ast mid = (get_term_type(y) == LitA) ? get_placeholder(q) : y;
	    itp = make(And,make(Equal,x,mid),mk_not(r));
	  }
	}
      }
    }
    else {
      if(get_term_type(q) == LitA){
	if(get_term_type(r) == LitA)
	  itp = mk_not(p);
	else {
	  if(get_term_type(r) == LitB)
	    itp = make(Equal,get_placeholder(p),z);
	  else {
	    ast mid = (get_term_type(y) == LitA) ? get_placeholder(p) : y;
	    itp = make(And,make(Equal,z,mid),mk_not(r));
	  }
	}
      }
      else {
	if(get_term_type(r) == LitA){
	  ast xr = (get_term_type(x) == LitA) ? get_placeholder(p) : x;
	  ast zr = (get_term_type(z) == LitA) ? get_placeholder(q) : z;
	  itp = mk_not(make(Equal,xr,zr));
	}
	else {
	  LitType xt = get_term_type(x);
	  LitType zt = get_term_type(z);
	  if(xt == LitA && zt == LitB)
	    itp = make(And,make(Equal,x,get_placeholder(p)),mk_not(r));
	  else if(zt == LitA && xt == LitB)
	    itp = make(And,make(Equal,z,get_placeholder(q)),mk_not(r));
	  else
	    itp = mk_true();
	}
      }
    }
    
    /* Resolve it with the premises */
    std::vector<ast> conc; conc.push_back(r); conc.push_back(mk_not(q));
    itp = make_resolution(p,conc,itp,prem1);
    conc.pop_back();
    itp = make_resolution(q,conc,itp,prem2);
#endif


    return itp;

  }
  
  /** Make a congruence node. This takes derivations of |- x_i = y_i
      and produces |- f(x_1,...,x_n) = f(y_1,...,y_n) */
  
  virtual node make_congruence(const ast &p, const ast &con, const ast &prem1){
    ast x = arg(p,0), y = arg(p,1);
    ast itp;
    if(get_term_type(p) == LitA){
      if(get_term_type(con) == LitA)
	itp = mk_false();
      else
	itp = make_mixed_congruence(x, y, p, con, prem1);
    }
    else {
      if(get_term_type(con) == LitA)
	itp = mk_not(p);
      else{
	if(get_term_type(con) == LitB)
	  itp = mk_true();
	else
	  itp = make_mixed_congruence(x, y, p, con, prem1);
      }
    }
    std::vector<ast> conc; conc.push_back(con);
    itp = make_resolution(p,conc,itp,prem1);
    return itp;
  }

  int find_congruence_position(const ast &p, const ast &con){
      // find the argument position of x and y
    const ast &x = arg(p,0);
    const ast &y = arg(p,1);
    int nargs = num_args(arg(con,0));
    for(int i = 0; i < nargs; i++)
      if(x == arg(arg(con,0),i) && y == arg(arg(con,1),i))
	return i;
    throw proof_error();
  }

  /** Make a congruence node. This takes derivations of |- x_i1 = y_i1, |- x_i2 = y_i2,...
      and produces |- f(...x_i1...x_i2...) = f(...y_i1...y_i2...) */
  
  node make_congruence(const std::vector<ast> &p, const ast &con, const std::vector<ast> &prems){
    if(p.size() == 0)
      throw proof_error();
    if(p.size() == 1)
      return make_congruence(p[0],con,prems[0]);
    ast thing = con;
    ast res = mk_true();
    for(unsigned i = 0; i < p.size(); i++){
      int pos = find_congruence_position(p[i],thing);
      ast next = subst_in_arg_pos(pos,arg(p[i],1),arg(thing,0));
      ast goal = make(op(thing),arg(thing,0),next);
      ast equa = make_congruence(p[i],goal,prems[i]);
      if(i == 0)
	res = equa;
      else {
	ast trace = make(op(con),arg(con,0),arg(thing,0));
	ast equiv = make(Iff,trace,make(op(trace),arg(trace,0),next));
	ast foo = make_congruence(goal,equiv,equa);
	res = make_mp(equiv,res,foo);
      }
      thing = make(op(thing),next,arg(thing,1));
    }
    return res;
  }

  /* Interpolate a mixed congruence axiom. */

  virtual ast make_mixed_congruence(const ast &x, const ast &y, const ast &p, const ast &con, const ast &prem1){
    ast foo = p;
    std::vector<ast> conjs;
    switch(get_term_type(foo)){
    case LitA:
      break;
    case LitB:
      foo = mk_true();
      break;
    case LitMixed:
      conjs.push_back(foo);
      foo = get_placeholder(foo);
    }
    // find the argument position of x and y
    int pos = -1;
    int nargs = num_args(arg(con,0));
    for(int i = 0; i < nargs; i++)
      if(x == arg(arg(con,0),i) && y == arg(arg(con,1),i))
	pos = i;
    if(pos == -1)
      throw proof_error();
    ast bar = make(cong,foo,make_int(rational(pos)),get_placeholder(mk_not(con)));
    conjs.push_back(mk_not(con));
    return make_contra_node(bar,conjs);
#if 0
    ast A_term = x;
    ast f_A_term = arg(con,0);
    if(get_term_type(y) == LitA){
      A_term = y;
      f_A_term = arg(con,1);
    }


    ast res = make(Equal,f_A_term,subst_in_arg_pos(pos,get_placeholder(make(Equal,x,y)),f_A_term));
    res = make(And,res,mk_not(con));
    return res;
#endif
  }

  ast subst_in_arg_pos(int pos, ast term, ast app){
    std::vector<ast> args;
    get_args(app,args);
    args[pos] = term;
    return clone(app,args);
  }

  /** Make a farkas proof node. */

  virtual node make_farkas(ast con, const std::vector<node> &prems, const std::vector<ast> &prem_cons,
			   const std::vector<ast> &coeffs){

    /* Compute the interpolant for the clause */

    ast zero = make_int("0");
    std::vector<ast> conjs;
    ast thing = make(Leq,zero,zero);
    for(unsigned i = 0; i < prem_cons.size(); i++){
      const ast &lit = prem_cons[i];
      if(get_term_type(lit) == LitA)
	linear_comb(thing,coeffs[i],lit);
    }
    thing = simplify_ineq(thing);
    for(unsigned i = 0; i < prem_cons.size(); i++){
      const ast &lit = prem_cons[i];
      if(get_term_type(lit) == LitMixed){
	thing = make(sum,thing,coeffs[i],get_placeholder(lit));
	conjs.push_back(lit);
      }
    }
    thing = make_contra_node(thing,conjs);

    /* Resolve it with the premises */
    std::vector<ast> conc; conc.resize(prem_cons.size());
    for(unsigned i = 0; i < prem_cons.size(); i++)
      conc[prem_cons.size()-i-1] = prem_cons[i];
    for(unsigned i = 0; i < prem_cons.size(); i++){
      thing = make_resolution(prem_cons[i],conc,thing,prems[i]);
      conc.pop_back();
    }
    return thing;
  }

  /** Set P to P + cQ, where P and Q are linear inequalities. Assumes P is 0 <= y or 0 < y. */

  void linear_comb(ast &P, const ast &c, const ast &Q){
    ast Qrhs;
    bool strict = op(P) == Lt;
    if(is_not(Q)){
      ast nQ = arg(Q,0);
      switch(op(nQ)){
      case Gt:
	Qrhs = make(Sub,arg(nQ,1),arg(nQ,0));
	break;
      case Lt: 
	Qrhs = make(Sub,arg(nQ,0),arg(nQ,1));
	break;
      case Geq:
	Qrhs = make(Sub,arg(nQ,1),arg(nQ,0));
	strict = true;
	break;
      case Leq: 
	Qrhs = make(Sub,arg(nQ,0),arg(nQ,1));
	strict = true;
	break;
      default:
	throw proof_error();
      }
    }
    else {
      switch(op(Q)){
      case Leq:
	Qrhs = make(Sub,arg(Q,1),arg(Q,0));
	break;
      case Geq: 
	Qrhs = make(Sub,arg(Q,0),arg(Q,1));
	break;
      case Lt:
	Qrhs = make(Sub,arg(Q,1),arg(Q,0));
	strict = true;
	break;
      case Gt: 
	Qrhs = make(Sub,arg(Q,0),arg(Q,1));
	strict = true;
	break;
      default:
	throw proof_error();
      }
    }
    Qrhs = make(Times,c,Qrhs);
    if(strict)
      P = make(Lt,arg(P,0),make(Plus,arg(P,1),Qrhs));
    else
      P = make(Leq,arg(P,0),make(Plus,arg(P,1),Qrhs));
  }
  
  /* Make an axiom instance of the form |- x<=y, y<= x -> x =y */
  virtual node make_leq2eq(ast x, ast y, const ast &xleqy, const ast &yleqx){
    ast con = make(Equal,x,y);
    ast itp;
    switch(get_term_type(con)){
    case LitA:
      itp =  mk_false();
      break;
    case LitB:
      itp =  mk_true();
      break;
    default: { // mixed equality
      std::vector<ast> conjs; conjs.resize(3);
      conjs[0] = mk_not(con);
      conjs[1] = xleqy;
      conjs[2] = yleqx;
      itp = make_contra_node(make(leq2eq,
			     get_placeholder(mk_not(con)),
			     get_placeholder(xleqy),
			     get_placeholder(yleqx)),
			     conjs,1);
    }
    }
    return itp;
  }

  /* Make an axiom instance of the form |- x = y -> x <= y */
  virtual node make_eq2leq(ast x, ast y, const ast &xleqy){
    ast itp;
    switch(get_term_type(xleqy)){
    case LitA:
      itp =  mk_false();
      break;
    case LitB:
      itp =  mk_true();
      break;
    default: { // mixed equality
#if 0
      ast mid = get_placeholder(make(Equal,x,y));
      if(get_term_type(y) == LitA){
	std::swap(x,y);
	mid = make(Sub,x,mid);
      }
      else {
	mid = make(Sub,mid,x);
      }
      ast zleqmid = make(Leq,make_int("0"),mid);
      ast fark = make(contra,make_int("1"),mk_not(xleqy));
      itp = make(And,zleqmid,fark);
#endif
      std::vector<ast> conjs; conjs.resize(2);
      conjs[0] = make(Equal,x,y);
      conjs[1] = mk_not(xleqy);
      itp = make(eq2leq,get_placeholder(conjs[0]),get_placeholder(conjs[1]));
      itp = make_contra_node(itp,conjs);
    }
    }
    return itp;
  }


  /* Make an inference of the form t <= c |- t/d <= floor(c/d) where t
     is an affine term divisble by d and c is an integer constant */
  virtual node make_cut_rule(const ast &tleqc, const ast &d, const ast &con, const ast &prem){
    ast itp = mk_false();
    switch(get_term_type(con)){
    case LitA:
      itp =  mk_false();
      break;
    case LitB:
      itp =  mk_true();
      break;
    default: {
      std::vector<ast> conjs; conjs.resize(2);
      conjs[0] = tleqc;
      conjs[1] = mk_not(con);
      itp = make(sum,get_placeholder(conjs[0]),d,get_placeholder(conjs[1]));
      itp = make_contra_node(itp,conjs);
#if 0
      ast t = arg(tleqc,0);
      ast c = arg(tleqc,1);
      ast thing = make(contra,make_int("1"),mk_not(con));
      itp = make(And,make(Leq,make_int("0"),make(Idiv,get_placeholder(tleqc),d)),thing);
#endif
    }
    }
    std::vector<ast> conc; conc.push_back(con);
    itp = make_resolution(tleqc,conc,itp,prem);
    return itp;
  }


  
  // create a fresh variable for localization
  ast fresh_localization_var(const ast &term, int frame){
    std::ostringstream s;
    s << "%" << (localization_vars.size());
    ast var = make_var(s.str().c_str(),get_type(term));
    pv->sym_range(sym(var)) = pv->range_full(); // make this variable global
    localization_vars.push_back(LocVar(var,term,frame));
    return var;
  }
  
  struct LocVar {                           // localization vars
    ast var;                                // a fresh variable
    ast term;                               // term it represents
    int frame;                              // frame in which it's defined
    LocVar(ast v, ast t, int f){var=v;term=t;frame=f;}
  };

  std::vector<LocVar> localization_vars;         // localization vars in order of creation
  hash_map<ast,ast> localization_map;            // maps terms to their localization vars
  hash_map<ast,ast> localization_pf_map;         // maps terms to proofs of their localizations

  /* "localize" a term e to a given frame range, creating new symbols to
     represent non-local subterms. This returns the localized version e_l,
     as well as a proof thet e_l = l. 
  */

  ast make_refl(const ast &e){
    return mk_true(); // TODO: is this right?
  }


  ast make_equiv(const ast &x, const ast &y){
    if(get_type(x) == bool_type())
      return make(Iff,x,y);
    else
      return make(Equal,x,y);
  }

  ast localize_term(ast e, const prover::range &rng, ast &pf){
    ast orig_e = e;
    pf = make_refl(e);  // proof that e = e

    prover::range erng = pv->ast_scope(e);
    if(!(erng.lo > erng.hi) && pv->ranges_intersect(pv->ast_scope(e),rng)){
      return e; // this term occurs in range, so it's O.K.
    }

    hash_map<ast,ast>::iterator it = localization_map.find(e);
    if(it != localization_map.end()){
      pf = localization_pf_map[e];
      return it->second;
    }

    // if is is non-local, we must first localize the arguments to
    // the range of its function symbol
    
    int nargs = num_args(e);
    if(nargs > 0 /*  && (!is_local(e) || flo <= hi || fhi >= lo) */){
      prover::range frng = rng;
      if(op(e) == Uninterpreted){
	symb f = sym(e);
	prover::range srng = pv->sym_range(f);
	if(pv->ranges_intersect(srng,rng)) // localize to desired range if possible
	  frng = pv->range_glb(srng,rng);
      }
      std::vector<ast> largs(nargs);
      std::vector<ast> eqs;
      std::vector<ast> pfs;
      for(int i = 0; i < nargs; i++){
	ast argpf;
	largs[i] = localize_term(arg(e,i),frng,argpf);
	frng = pv->range_glb(frng,pv->ast_scope(largs[i]));
	if(largs[i] != arg(e,i)){
	  eqs.push_back(make_equiv(largs[i],arg(e,i)));
	  pfs.push_back(argpf);
	}
      }

      e = clone(e,largs);
      if(pfs.size())
	pf = make_congruence(eqs,make_equiv(e,orig_e),pfs);
      // assert(is_local(e));
    }

    if(pv->ranges_intersect(pv->ast_scope(e),rng))
      return e; // this term occurs in range, so it's O.K.

    // choose a frame for the constraint that is close to range
    int frame = pv->range_near(pv->ast_scope(e),rng);

    ast new_var = fresh_localization_var(e,frame);
    localization_map[e] = new_var;
    std::vector<ast> foo; foo.push_back(make_equiv(new_var,e));
    ast bar = make_assumption(frame,foo);
    pf = make_transitivity(new_var,e,orig_e,bar,pf);
    localization_pf_map[orig_e] = pf;
    return new_var;
  }

  node make_resolution(ast pivot, node premise1, node premise2) {
    std::vector<ast> lits;
    return make_resolution(pivot,lits,premise1,premise2);
  }  

  ast resolve_with_quantifier(const ast &pivot1, const ast &conj1,
			      const ast &pivot2, const ast &conj2){
   if(is_not(arg(pivot1,1)))
      return resolve_with_quantifier(pivot2,conj2,pivot1,conj1);
    ast eqpf;
    ast P = arg(pivot1,1);
    ast Ploc = localize_term(P, rng, eqpf);
    ast pPloc = make_hypothesis(Ploc);
    ast pP = make_mp(make(Iff,Ploc,P),pPloc,eqpf);
    ast rP = make_resolution(P,conj1,pP);
    ast nP = mk_not(P);
    ast nPloc = mk_not(Ploc);
    ast neqpf = make_congruence(make(Iff,Ploc,P),make(Iff,nPloc,nP),eqpf);
    ast npPloc = make_hypothesis(nPloc);
    ast npP = make_mp(make(Iff,nPloc,nP),npPloc,neqpf);
    ast nrP = make_resolution(nP,conj2,npP);
    ast res = make_resolution(Ploc,rP,nrP);
    return res;
  }

  ast get_contra_coeff(const ast &f){
    ast c = arg(f,0);
    // if(!is_not(arg(f,1)))
    // c = make(Uminus,c);
    return c;
  }

  ast my_or(const ast &a, const ast &b){
    return mk_or(a,b);
  }

  ast my_and(const ast &a, const ast &b){
    return mk_and(a,b);
  }
  
  ast my_implies(const ast &a, const ast &b){
    return mk_implies(a,b);
  }

  ast my_or(const std::vector<ast> &a){
    return mk_or(a);
  }

  ast my_and(const std::vector<ast> &a){
    return mk_and(a);
  }

  ast get_lit_atom(const ast &l){
    if(op(l) == Not)
      return arg(l,0);
    return l;
  }

  bool is_placeholder(const ast &e){
    if(op(e) == Uninterpreted){
      std::string name = string_of_symbol(sym(e));
      if(name.size() > 2 && name[0] == '@' and name[1] == 'p')
	return true;
    }
    return false;
  }

public:
  iz3proof_itp_impl(prover *p, const prover::range &r, bool w)
    : iz3proof_itp(*p)
  {
    pv = p;
    rng = r;
    weak = w;
    type boolintbooldom[3] = {bool_type(),int_type(),bool_type()};
    type booldom[1] = {bool_type()};
    type boolbooldom[2] = {bool_type(),bool_type()};
    type boolboolbooldom[3] = {bool_type(),bool_type(),bool_type()};
    contra = function("@contra",2,boolbooldom,bool_type());
    sum = function("@sum",3,boolintbooldom,bool_type());
    rotate_sum = function("@rotsum",2,boolbooldom,bool_type());
    leq2eq = function("@leq2eq",3,boolboolbooldom,bool_type());
    eq2leq = function("@eq2leq",2,boolbooldom,bool_type());
    cong = function("@cong",3,boolintbooldom,bool_type());
    exmid = function("@exmid",3,boolboolbooldom,bool_type());
    symm = function("@symm",1,booldom,bool_type());
    epsilon = make_var("@eps",int_type());
    modpon = function("@mp",3,boolboolbooldom,bool_type());
    no_proof = make_var("@nop",bool_type());
  }

};

iz3proof_itp *iz3proof_itp::create(prover *p, const prover::range &r, bool w){
  return new iz3proof_itp_impl(p,r,w);
}
