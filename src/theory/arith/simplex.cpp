
#include "theory/arith/simplex.h"

using namespace std;

using namespace CVC4;
using namespace CVC4::kind;

using namespace CVC4::theory;
using namespace CVC4::theory::arith;

const static uint64_t ACTIVITY_THRESHOLD = 100;

SimplexDecisionProcedure::Statistics::Statistics():
  d_statPivots("theory::arith::pivots",0),
  d_statUpdates("theory::arith::updates",0),
  d_statAssertUpperConflicts("theory::arith::AssertUpperConflicts", 0),
  d_statAssertLowerConflicts("theory::arith::AssertLowerConflicts", 0),
  d_statUpdateConflicts("theory::arith::UpdateConflicts", 0),
  d_statEjections("theory::arith::Ejections", 0),
  d_statUnEjections("theory::arith::UnEjections", 0),
  d_statEarlyConflicts("theory::arith::EarlyConflicts", 0),
  d_statEarlyConflictImprovements("theory::arith::EarlyConflictImprovements", 0),
  d_selectInitialConflictTime("theory::arith::selectInitialConflictTime"),
  d_pivotsAfterConflict("theory::arith::pivotsAfterConflict", 0),
  d_checksWithWastefulPivots("theory::arith::checksWithWastefulPivots", 0),
  d_pivotTime("theory::arith::pivotTime")
{
  StatisticsRegistry::registerStat(&d_statPivots);
  StatisticsRegistry::registerStat(&d_statUpdates);
  StatisticsRegistry::registerStat(&d_statAssertUpperConflicts);
  StatisticsRegistry::registerStat(&d_statAssertLowerConflicts);
  StatisticsRegistry::registerStat(&d_statUpdateConflicts);
  StatisticsRegistry::registerStat(&d_statEjections);
  StatisticsRegistry::registerStat(&d_statUnEjections);
  StatisticsRegistry::registerStat(&d_statEarlyConflicts);
  StatisticsRegistry::registerStat(&d_statEarlyConflictImprovements);
  StatisticsRegistry::registerStat(&d_selectInitialConflictTime);

  StatisticsRegistry::registerStat(&d_pivotsAfterConflict);
  StatisticsRegistry::registerStat(&d_checksWithWastefulPivots);
  StatisticsRegistry::registerStat(&d_pivotTime);
}

SimplexDecisionProcedure::Statistics::~Statistics(){
  StatisticsRegistry::unregisterStat(&d_statPivots);
  StatisticsRegistry::unregisterStat(&d_statUpdates);
  StatisticsRegistry::unregisterStat(&d_statAssertUpperConflicts);
  StatisticsRegistry::unregisterStat(&d_statAssertLowerConflicts);
  StatisticsRegistry::unregisterStat(&d_statUpdateConflicts);
  StatisticsRegistry::unregisterStat(&d_statEjections);
  StatisticsRegistry::unregisterStat(&d_statUnEjections);
  StatisticsRegistry::unregisterStat(&d_statEarlyConflicts);
  StatisticsRegistry::unregisterStat(&d_statEarlyConflictImprovements);
  StatisticsRegistry::unregisterStat(&d_selectInitialConflictTime);

  StatisticsRegistry::unregisterStat(&d_pivotsAfterConflict);
  StatisticsRegistry::unregisterStat(&d_checksWithWastefulPivots);
  StatisticsRegistry::unregisterStat(&d_pivotTime);
}


/*
void SimplexDecisionProcedure::ejectInactiveVariables(){
  return; //die die die

  for(ArithVarSet::iterator i = d_tableau.begin(), end = d_tableau.end(); i != end;){
    ArithVar variable = *i;
    ++i;
    Assert(d_basicManager.isMember(variable));

    if(d_basicManager.isMember(variable) && shouldEject(variable)){
      Debug("decay") << "ejecting basic " << variable << endl;
      ++(d_statistics.d_statEjections);
      d_tableau.ejectBasic(variable);
    }
  }
}
*/

/*
void SimplexDecisionProcedure::reinjectVariable(ArithVar x){
  ++(d_statistics.d_statUnEjections);

  d_tableau.reinjectBasic(x);

  DeltaRational safeAssignment = computeRowValue(x, true);
  DeltaRational assignment = computeRowValue(x, false);
  d_partialModel.setAssignment(x,safeAssignment,assignment);
}
*/

/*
bool SimplexDecisionProcedure::shouldEject(ArithVar var){
  if(d_partialModel.hasEitherBound(var)){
    return false;
  }else if(d_tableau.isEjected(var)) {
    return false;
  }else if(!d_partialModel.hasEverHadABound(var)){
    return true;
  }else if(d_activityMonitor[var] >= ACTIVITY_THRESHOLD){
    return false;
  }
  return false;
}
*/

/* procedure AssertLower( x_i >= c_i ) */
bool SimplexDecisionProcedure::AssertLower(ArithVar x_i, const DeltaRational& c_i, TNode original){
  Debug("arith") << "AssertLower(" << x_i << " " << c_i << ")"<< std::endl;

  /*
  if(d_basicManager.isMember(x_i) && d_tableau.isEjected(x_i)){
    reinjectVariable(x_i);
  }
  */

  if(d_partialModel.belowLowerBound(x_i, c_i, false)){
    return false; //sat
  }
  if(d_partialModel.aboveUpperBound(x_i, c_i, true)){
    Node ubc = d_partialModel.getUpperConstraint(x_i);
    Node conflict =  NodeManager::currentNM()->mkNode(AND, ubc, original);
    d_out->conflict(conflict);
    Debug("arith") << "AssertLower conflict " << conflict << endl;
    ++(d_statistics.d_statAssertLowerConflicts);
    return true;
  }

  d_partialModel.setLowerConstraint(x_i,original);
  d_partialModel.setLowerBound(x_i, c_i);
  d_activityMonitor[x_i] = 0;

  if(!d_basicManager.isMember(x_i)){
    if(d_partialModel.getAssignment(x_i) < c_i){
      update(x_i, c_i);
    }
  }else{
    checkBasicVariable(x_i);
  }

  return false;
}

/* procedure AssertUpper( x_i <= c_i) */
bool SimplexDecisionProcedure::AssertUpper(ArithVar x_i, const DeltaRational& c_i, TNode original){

  Debug("arith") << "AssertUpper(" << x_i << " " << c_i << ")"<< std::endl;
  /*
  if(d_basicManager.isMember(x_i) && d_tableau.isEjected(x_i)){
    reinjectVariable(x_i);
  }
  */

  if(d_partialModel.aboveUpperBound(x_i, c_i, false) ){ // \upperbound(x_i) <= c_i
    return false; //sat
  }
  if(d_partialModel.belowLowerBound(x_i, c_i, true)){// \lowerbound(x_i) > c_i
    Node lbc = d_partialModel.getLowerConstraint(x_i);
    Node conflict =  NodeManager::currentNM()->mkNode(AND, lbc, original);
    Debug("arith") << "AssertUpper conflict " << conflict << endl;
    ++(d_statistics.d_statAssertUpperConflicts);
    d_out->conflict(conflict);
    return true;
  }

  d_partialModel.setUpperConstraint(x_i,original);
  d_partialModel.setUpperBound(x_i, c_i);

  d_activityMonitor[x_i] = 0;

  if(!d_basicManager.isMember(x_i)){
    if(d_partialModel.getAssignment(x_i) > c_i){
      update(x_i, c_i);
    }
  }else{
    checkBasicVariable(x_i);
  }
  d_partialModel.printModel(x_i);
  return false;
}


/* procedure AssertLower( x_i == c_i ) */
bool SimplexDecisionProcedure::AssertEquality(ArithVar x_i, const DeltaRational& c_i, TNode original){

  Debug("arith") << "AssertEquality(" << x_i << " " << c_i << ")"<< std::endl;

  /*
  if(d_basicManager.isMember(x_i) && d_tableau.isEjected(x_i)){
    reinjectVariable(x_i);
  }
  */

  // u_i <= c_i <= l_i
  // This can happen if both c_i <= x_i and x_i <= c_i are in the system.
  if(d_partialModel.belowLowerBound(x_i, c_i, false) &&
     d_partialModel.aboveUpperBound(x_i, c_i, false)){
    return false; //sat
  }

  if(d_partialModel.aboveUpperBound(x_i, c_i, true)){
    Node ubc = d_partialModel.getUpperConstraint(x_i);
    Node conflict =  NodeManager::currentNM()->mkNode(AND, ubc, original);
    d_out->conflict(conflict);
    Debug("arith") << "AssertLower conflict " << conflict << endl;
    return true;
  }

  if(d_partialModel.belowLowerBound(x_i, c_i, true)){
    Node lbc = d_partialModel.getLowerConstraint(x_i);
    Node conflict =  NodeManager::currentNM()->mkNode(AND, lbc, original);
    Debug("arith") << "AssertUpper conflict " << conflict << endl;
    d_out->conflict(conflict);
    return true;
  }

  d_partialModel.setLowerConstraint(x_i,original);
  d_partialModel.setLowerBound(x_i, c_i);

  d_partialModel.setUpperConstraint(x_i,original);
  d_partialModel.setUpperBound(x_i, c_i);
  d_activityMonitor[x_i] = 0;

  if(!d_basicManager.isMember(x_i)){
    if(!(d_partialModel.getAssignment(x_i) == c_i)){
      update(x_i, c_i);
    }
  }else{
    checkBasicVariable(x_i);
  }

  return false;
}

void SimplexDecisionProcedure::update(ArithVar x_i, const DeltaRational& v){
  Assert(!d_basicManager.isMember(x_i));
  DeltaRational assignment_x_i = d_partialModel.getAssignment(x_i);
  ++(d_statistics.d_statUpdates);

  Debug("arith") <<"update " << x_i << ": "
                 << assignment_x_i << "|-> " << v << endl;
  DeltaRational diff = v - assignment_x_i;

  for(ArithVarSet::iterator basicIter = d_tableau.begin();
      basicIter != d_tableau.end();
      ++basicIter){
    ArithVar x_j = *basicIter;
    ReducedRowVector& row_j = d_tableau.lookup(x_j);

    if(row_j.has(x_i)){
      const Rational& a_ji = row_j.lookup(x_i);

      const DeltaRational& assignment = d_partialModel.getAssignment(x_j);
      DeltaRational  nAssignment = assignment+(diff * a_ji);
      d_partialModel.setAssignment(x_j, nAssignment);

      d_activityMonitor[x_j] += 1;

      checkBasicVariable(x_j);
    }
  }

  d_partialModel.setAssignment(x_i, v);

  if(Debug.isOn("paranoid:check_tableau")){
    checkTableau();
  }
}

void SimplexDecisionProcedure::pivotAndUpdate(ArithVar x_i, ArithVar x_j, DeltaRational& v){
  Assert(x_i != x_j);

  TimerStat::CodeTimer codeTimer(d_statistics.d_pivotTime);

  if(Debug.isOn("arith::pivotAndUpdate")){
    Debug("arith::pivotAndUpdate") << "x_i " << "|->" << x_j << endl;
    ReducedRowVector& row_k = d_tableau.lookup(x_i);
    for(ReducedRowVector::NonZeroIterator varIter = row_k.beginNonZero();
        varIter != row_k.endNonZero();
        ++varIter){

      ArithVar var = varIter->first;
      const Rational& coeff = varIter->second;
      DeltaRational beta = d_partialModel.getAssignment(var);
      Debug("arith::pivotAndUpdate") << var << beta << coeff;
      if(d_partialModel.hasLowerBound(var)){
        Debug("arith::pivotAndUpdate") << "(lb " << d_partialModel.getLowerBound(var) << ")";
      }
      if(d_partialModel.hasUpperBound(var)){
        Debug("arith::pivotAndUpdate") << "(up " << d_partialModel.getUpperBound(var)
                                       << ")";
      }
      Debug("arith::pivotAndUpdate") << endl;
    }
    Debug("arith::pivotAndUpdate") << "end row"<< endl;
  }


  ReducedRowVector& row_i = d_tableau.lookup(x_i);
  const Rational& a_ij = row_i.lookup(x_j);


  const DeltaRational& betaX_i = d_partialModel.getAssignment(x_i);

  Rational inv_aij = a_ij.inverse();
  DeltaRational theta = (v - betaX_i)*inv_aij;

  d_partialModel.setAssignment(x_i, v);


  DeltaRational tmp = d_partialModel.getAssignment(x_j) + theta;
  d_partialModel.setAssignment(x_j, tmp);

  ArithVarSet::iterator basicIter = d_tableau.begin(), end = d_tableau.end();
  for(; basicIter != end; ++basicIter){
    ArithVar x_k = *basicIter;
    ReducedRowVector& row_k = d_tableau.lookup(x_k);

    if(x_k != x_i && row_k.has(x_j)){
      const Rational& a_kj = row_k.lookup(x_j);
      DeltaRational nextAssignment = d_partialModel.getAssignment(x_k) + (theta * a_kj);
      d_partialModel.setAssignment(x_k, nextAssignment);

      d_activityMonitor[x_j] += 1;

      checkBasicVariable(x_k);
    }
  }

  // Pivots
  ++(d_statistics.d_statPivots);
  if( d_foundAConflict ){
    ++d_pivotsSinceConflict;
    if(d_pivotsSinceConflict == 1){
      ++(d_statistics.d_checksWithWastefulPivots);
    }
    ++(d_statistics.d_pivotsAfterConflict);
  }

  d_tableau.pivot(x_i, x_j);

  checkBasicVariable(x_j);

  // Debug found conflict
  if( !d_foundAConflict ){
    DeltaRational beta_j = d_partialModel.getAssignment(x_j);

    if(d_partialModel.belowLowerBound(x_j, beta_j, true)){
      if(selectSlackBelow(x_j) == ARITHVAR_SENTINEL ){
        d_foundAConflict = true;
      }
    }else if(d_partialModel.aboveUpperBound(x_j, beta_j, true)){
      if(selectSlackAbove(x_j) == ARITHVAR_SENTINEL ){
        d_foundAConflict = true;
      }
    }
  }

  if(Debug.isOn("tableau")){
    d_tableau.printTableau();
  }
}

ArithVar SimplexDecisionProcedure::selectSmallestInconsistentVar(){
  Debug("arith_update") << "selectSmallestInconsistentVar()" << endl;
  Debug("arith_update") << "possiblyInconsistent.size()"
                        << d_possiblyInconsistent.size() << endl;

  if(d_pivotStage){
    while(!d_griggioRuleQueue.empty()){
      ArithVar var = d_griggioRuleQueue.top().first;
      Debug("arith_update") << "possiblyInconsistentGriggio var" << var << endl;
      if(d_basicManager.isMember(var)){
        if(!d_partialModel.assignmentIsConsistent(var)){
          return var;
        }
      }
      d_griggioRuleQueue.pop();
    }
  }else{
    while(!d_possiblyInconsistent.empty()){
      ArithVar var = d_possiblyInconsistent.top();
      Debug("arith_update") << "possiblyInconsistent var" << var << endl;
      if(d_basicManager.isMember(var)){
        if(!d_partialModel.assignmentIsConsistent(var)){
          return var;
        }
      }
      d_possiblyInconsistent.pop();
    }
  }
  return ARITHVAR_SENTINEL;
}

template <bool above>
ArithVar SimplexDecisionProcedure::selectSlack(ArithVar x_i){
  ReducedRowVector& row_i = d_tableau.lookup(x_i);

  ArithVar slack = ARITHVAR_SENTINEL;
  uint32_t numRows = std::numeric_limits<uint32_t>::max();

  for(ReducedRowVector::NonZeroIterator nbi = row_i.beginNonZero(), end = row_i.endNonZero();
      nbi != end; ++nbi){
    ArithVar nonbasic = getArithVar(*nbi);
    if(nonbasic == x_i) continue;

    const Rational& a_ij = nbi->second;
    int cmp = a_ij.cmp(d_constants.d_ZERO);
    if(above){ // beta(x_i) > u_i
      if( cmp < 0 && d_partialModel.strictlyBelowUpperBound(nonbasic)){
        if(d_pivotStage){
          if(d_tableau.getRowCount(nonbasic) < numRows){
            slack = nonbasic;
            numRows = d_tableau.getRowCount(nonbasic);
          }
        }else{
          slack = nonbasic; break;
        }
      }else if( cmp > 0 && d_partialModel.strictlyAboveLowerBound(nonbasic)){
        if(d_pivotStage){
          if(d_tableau.getRowCount(nonbasic) < numRows){
            slack = nonbasic;
            numRows = d_tableau.getRowCount(nonbasic);
          }
        }else{
          slack = nonbasic; break;
        }
      }
    }else{ //beta(x_i) < l_i
      if(cmp > 0 && d_partialModel.strictlyBelowUpperBound(nonbasic)){
        if(d_pivotStage){
          if(d_tableau.getRowCount(nonbasic) < numRows){
            slack = nonbasic;
            numRows = d_tableau.getRowCount(nonbasic);
          }
        }else{
          slack = nonbasic; break;
        }
      }else if(cmp < 0 && d_partialModel.strictlyAboveLowerBound(nonbasic)){
        if(d_pivotStage){
          if(d_tableau.getRowCount(nonbasic) < numRows){
            slack = nonbasic;
            numRows = d_tableau.getRowCount(nonbasic);
          }
        }else{
          slack = nonbasic; break;
        }
      }
    }
  }

  return slack;
}

Node betterConflict(TNode x, TNode y){
  if(x.isNull()) return y;
  else if(y.isNull()) return x;
  else if(x.getNumChildren() <= y.getNumChildren()) return x;
  else return y;
}

Node SimplexDecisionProcedure::selectInitialConflict() {
  Node bestConflict = Node::null();

  TimerStat::CodeTimer codeTimer(d_statistics.d_selectInitialConflictTime);

  vector<VarDRatPair>  init;

  while( !d_griggioRuleQueue.empty()){
    ArithVar var = d_griggioRuleQueue.top().first;
    if(d_basicManager.isMember(var)){
      if(!d_partialModel.assignmentIsConsistent(var)){
        init.push_back( d_griggioRuleQueue.top());
      }
    }
    d_griggioRuleQueue.pop();
  }

  int conflictChanges = 0;

  for(vector<VarDRatPair>::iterator i=init.begin(), end=init.end(); i != end; ++i){
    ArithVar x_i = (*i).first;
    d_griggioRuleQueue.push(*i);

    Node possibleConflict = checkBasicForConflict(x_i);
    if(!possibleConflict.isNull()){
      Node better = betterConflict(bestConflict, possibleConflict);

      if(better != bestConflict){
        ++conflictChanges;
      }
      bestConflict = better;
      ++(d_statistics.d_statEarlyConflicts);
    }
  }
  if(conflictChanges > 1) ++(d_statistics.d_statEarlyConflictImprovements);
  return bestConflict;
}

Node SimplexDecisionProcedure::updateInconsistentVars(){
  if(d_griggioRuleQueue.empty()) return Node::null();

  d_foundAConflict = false;
  d_pivotsSinceConflict = 0;

  Node possibleConflict = Node::null();
  if(d_griggioRuleQueue.size() > 1){
    possibleConflict = selectInitialConflict();
  }
  if(possibleConflict.isNull()){
    possibleConflict = privateUpdateInconsistentVars();
  }

  Assert(!possibleConflict.isNull() || d_griggioRuleQueue.empty());
  Assert(!possibleConflict.isNull() || d_possiblyInconsistent.empty());
  d_pivotStage = true;

  while(!d_griggioRuleQueue.empty()){
    d_griggioRuleQueue.pop();
  }
  while(!d_possiblyInconsistent.empty()){
    d_possiblyInconsistent.pop();
  }

  return possibleConflict;
}

Node SimplexDecisionProcedure::checkBasicForConflict(ArithVar basic){

  Assert(d_basicManager.isMember(basic));
  const DeltaRational& beta = d_partialModel.getAssignment(basic);

  if(d_partialModel.belowLowerBound(basic, beta, true)){
    ArithVar x_j = selectSlackBelow(basic);
    if(x_j == ARITHVAR_SENTINEL ){
      return generateConflictBelow(basic);
    }
  }else if(d_partialModel.aboveUpperBound(basic, beta, true)){
    ArithVar x_j = selectSlackAbove(basic);
    if(x_j == ARITHVAR_SENTINEL ){
      return generateConflictAbove(basic);
    }
  }
  return Node::null();
}

//corresponds to Check() in dM06
Node SimplexDecisionProcedure::privateUpdateInconsistentVars(){
  Assert(d_pivotStage || d_griggioRuleQueue.empty());

  Debug("arith") << "updateInconsistentVars" << endl;

  uint32_t iteratationNum = 0;
  //static const int EJECT_FREQUENCY = 10;

  while(!d_pivotStage || iteratationNum <= d_numVariables){
    if(Debug.isOn("paranoid:check_tableau")){ checkTableau(); }

    ArithVar x_i = selectSmallestInconsistentVar();
    Debug("arith::update::select") << "selectSmallestInconsistentVar()=" << x_i << endl;
    if(x_i == ARITHVAR_SENTINEL){
      Debug("arith_update") << "No inconsistent variables" << endl;
      return Node::null(); //sat
    }

    ++iteratationNum;
    /*
    if(iteratationNum % EJECT_FREQUENCY == 0)
      ejectInactiveVariables();
    */

    DeltaRational beta_i = d_partialModel.getAssignment(x_i);
    ArithVar x_j = ARITHVAR_SENTINEL;

    if(d_partialModel.belowLowerBound(x_i, beta_i, true)){
      DeltaRational l_i = d_partialModel.getLowerBound(x_i);
      x_j = selectSlackBelow(x_i);
      if(x_j == ARITHVAR_SENTINEL ){
        ++(d_statistics.d_statUpdateConflicts);
        return generateConflictBelow(x_i); //unsat
      }
      pivotAndUpdate(x_i, x_j, l_i);

    }else if(d_partialModel.aboveUpperBound(x_i, beta_i, true)){
      DeltaRational u_i = d_partialModel.getUpperBound(x_i);
      x_j = selectSlackAbove(x_i);
      if(x_j == ARITHVAR_SENTINEL ){
        ++(d_statistics.d_statUpdateConflicts);
        return generateConflictAbove(x_i); //unsat
      }
      pivotAndUpdate(x_i, x_j, u_i);
    }
    Assert(x_j != ARITHVAR_SENTINEL);
    //Check to see if we already have a conflict with x_j to prevent wasteful work
    Node earlyConflict = checkBasicForConflict(x_j);
    if(!earlyConflict.isNull()){
      return earlyConflict;
    }
  }

  if(d_pivotStage && iteratationNum >= d_numVariables){
    while(!d_griggioRuleQueue.empty()){
      ArithVar var = d_griggioRuleQueue.top().first;
      if(d_basicManager.isMember(var)){
        d_possiblyInconsistent.push(var);
      }
      d_griggioRuleQueue.pop();
    }

    d_pivotStage = false;
    return privateUpdateInconsistentVars();
  }

  Unreachable();
}


Node SimplexDecisionProcedure::generateConflictAbove(ArithVar conflictVar){

  ReducedRowVector& row_i = d_tableau.lookup(conflictVar);

  NodeBuilder<> nb(kind::AND);
  TNode bound = d_partialModel.getUpperConstraint(conflictVar);

  Debug("arith")  << "generateConflictAbove "
                  << "conflictVar " << conflictVar
                  << " " << d_partialModel.getAssignment(conflictVar)
                  << " " << bound << endl;

  nb << bound;

  typedef ReducedRowVector::NonZeroIterator RowIterator;
  RowIterator nbi = row_i.beginNonZero(), end = row_i.endNonZero();

  for(; nbi != end; ++nbi){
    ArithVar nonbasic = getArithVar(*nbi);
    if(nonbasic == conflictVar) continue;

    const Rational& a_ij = nbi->second;

    Assert(a_ij != d_constants.d_ZERO);

    if(a_ij < d_constants.d_ZERO){
      bound =  d_partialModel.getUpperConstraint(nonbasic);
      Debug("arith") << "below 0 " << nonbasic << " "
                     << d_partialModel.getAssignment(nonbasic)
                     << " " << bound << endl;
      nb << bound;
    }else{
      bound =  d_partialModel.getLowerConstraint(nonbasic);
      Debug("arith") << " above 0 " << nonbasic << " "
                     << d_partialModel.getAssignment(nonbasic)
                     << " " << bound << endl;
      nb << bound;
    }
  }
  Node conflict = nb;
  return conflict;
}

Node SimplexDecisionProcedure::generateConflictBelow(ArithVar conflictVar){
  ReducedRowVector& row_i = d_tableau.lookup(conflictVar);

  NodeBuilder<> nb(kind::AND);
  TNode bound = d_partialModel.getLowerConstraint(conflictVar);

  Debug("arith") << "generateConflictBelow "
                 << "conflictVar " << conflictVar
                 << d_partialModel.getAssignment(conflictVar) << " "
                 << " " << bound << endl;
  nb << bound;

  typedef ReducedRowVector::NonZeroIterator RowIterator;
  RowIterator nbi = row_i.beginNonZero(), end = row_i.endNonZero();
  for(; nbi != end; ++nbi){
    ArithVar nonbasic = getArithVar(*nbi);
    if(nonbasic == conflictVar) continue;

    const Rational& a_ij = nbi->second;

    Assert(a_ij != d_constants.d_ZERO);

    if(a_ij < d_constants.d_ZERO){
      TNode bound = d_partialModel.getLowerConstraint(nonbasic);
      Debug("arith") << "Lower "<< nonbasic << " "
                     << d_partialModel.getAssignment(nonbasic) << " "
                     << bound << endl;

      nb << bound;
    }else{
      TNode bound = d_partialModel.getUpperConstraint(nonbasic);
      Debug("arith") << "Upper "<< nonbasic << " "
                     << d_partialModel.getAssignment(nonbasic) << " "
                     << bound << endl;

      nb << bound;
    }
  }
  Node conflict (nb.constructNode());
  return conflict;
}

/**
 * Computes the value of a basic variable using the current assignment.
 */
DeltaRational SimplexDecisionProcedure::computeRowValue(ArithVar x, bool useSafe){
  Assert(d_basicManager.isMember(x));
  DeltaRational sum = d_constants.d_ZERO_DELTA;

  ReducedRowVector& row = d_tableau.lookup(x);
  for(ReducedRowVector::NonZeroIterator i = row.beginNonZero(), end = row.endNonZero();
      i != end;++i){
    ArithVar nonbasic = getArithVar(*i);
    if(nonbasic == row.basic()) continue;
    const Rational& coeff = getCoefficient(*i);

    const DeltaRational& assignment = d_partialModel.getAssignment(nonbasic, useSafe);
    sum = sum + (assignment * coeff);
  }
  return sum;
}


void SimplexDecisionProcedure::checkBasicVariable(ArithVar basic){
  Assert(d_basicManager.isMember(basic));
  if(!d_partialModel.assignmentIsConsistent(basic)){
    if(d_pivotStage){
      const DeltaRational& beta = d_partialModel.getAssignment(basic);
      DeltaRational diff = d_partialModel.belowLowerBound(basic,beta,true) ?
        d_partialModel.getLowerBound(basic) - beta:
        beta - d_partialModel.getUpperBound(basic);
      d_griggioRuleQueue.push(make_pair(basic,diff));
    }else{
      d_possiblyInconsistent.push(basic);
    }
  }
}

/**
 * This check is quite expensive.
 * It should be wrapped in a Debug.isOn() guard.
 *   if(Debug.isOn("paranoid:check_tableau")){
 *      checkTableau();
 *   }
 */
void SimplexDecisionProcedure::checkTableau(){

  for(ArithVarSet::iterator basicIter = d_tableau.begin();
      basicIter != d_tableau.end(); ++basicIter){
    ArithVar basic = *basicIter;
    ReducedRowVector& row_k = d_tableau.lookup(basic);
    DeltaRational sum;
    Debug("paranoid:check_tableau") << "starting row" << basic << endl;
    for(ReducedRowVector::NonZeroIterator nonbasicIter = row_k.beginNonZero();
        nonbasicIter != row_k.endNonZero();
        ++nonbasicIter){
      ArithVar nonbasic = nonbasicIter->first;
      if(basic == nonbasic) continue;

      const Rational& coeff = nonbasicIter->second;
      DeltaRational beta = d_partialModel.getAssignment(nonbasic);
      Debug("paranoid:check_tableau") << nonbasic << beta << coeff<<endl;
      sum = sum + (beta*coeff);
    }
    DeltaRational shouldBe = d_partialModel.getAssignment(basic);
    Debug("paranoid:check_tableau") << "ending row" << sum
                                    << "," << shouldBe << endl;

    Assert(sum == shouldBe);
  }
}

