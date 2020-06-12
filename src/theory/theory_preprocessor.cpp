/*********************                                                        */
/*! \file theory_preprocessor.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Dejan Jovanovic, Morgan Deters, Andrew Reynolds
 ** This file is part of the CVC4 project.
 ** Copyright (c) 2009-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved.  See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** \brief The theory preprocessor
 **/

#include "theory/theory_preprocessor.h"

#include "expr/lazy_proof.h"
#include "theory/logic_info.h"
#include "theory/rewriter.h"
#include "theory/theory_engine.h"

using namespace std;

namespace CVC4 {
namespace theory {

TheoryPreprocessor::TheoryPreprocessor(TheoryEngine& engine,
                                       RemoveTermFormulas& tfr)
    : d_engine(engine),
      d_logicInfo(engine.getLogicInfo()),
      d_ppCache(),
      d_tfr(tfr)
{
}

TheoryPreprocessor::~TheoryPreprocessor() {}

void TheoryPreprocessor::clearCache() { d_ppCache.clear(); }

void TheoryPreprocessor::preprocess(TNode node,
                                    preprocessing::AssertionPipeline& lemmas,
                                    bool doTheoryPreprocess,
                                    LazyCDProof* lp)
{
  // Run theory preprocessing, maybe
  Node ppNode = doTheoryPreprocess ? theoryPreprocess(node, lp) : Node(node);

  // Remove the ITEs
  Trace("te-tform-rm") << "Remove term formulas from " << ppNode << std::endl;
  lemmas.push_back(ppNode);
  lemmas.updateRealAssertionsEnd();
  // TODO: pass lp to run here
  d_tfr.run(lemmas.ref(), lemmas.getIteSkolemMap(), false, nullptr);
  Trace("te-tform-rm") << "..done " << lemmas[0] << std::endl;

  // justify the preprocessing step
  if (lp != nullptr)
  {
    // currently this is a trusted step that combines theory preprocessing and
    // term formula removal.
    if (!CDProof::isSame(node, lemmas[0]))
    {
      std::vector<Node> pfChildren;
      pfChildren.push_back(node);
      std::vector<Node> pfArgs;
      pfArgs.push_back(lemmas[0]);
      lp->addStep(lemmas[0], PfRule::THEORY_PREPROCESS, pfChildren, pfArgs);
    }
#if 0
    for (size_t i = 1, lsize = lemmas.size(); i < lsize; ++i)
    {
      // the witness form of other lemmas should rewrite to true
      // For example, if (lambda y. t[y]) has skolem k, then this lemma is:
      //   forall x. k(x)=t[x]
      // whose witness form rewrites
      //   forall x. (lambda y. t[y])(x)=t[x] --> forall x. t[x]=t[x] --> true
      std::vector<Node> pfChildren;
      std::vector<Node> pfArgs;
      pfArgs.push_back(lemmas[i]);
      Trace("te-tf-check") << "Checking additional lemma..." << std::endl;
      Node cp = d_pchecker->checkDebug(PfRule::MACRO_SR_PRED_INTRO, pfChildren, pfArgs, lemmas[i], "te-tf-check");
      Trace("te-tf-check") << "...result: " << cp << std::endl;
      if (cp.isNull())
      {
        Node wt = SkolemManager::getWitnessForm(lemmas[i]);
        wt = Rewriter::rewrite(wt);
        Trace("te-tf-check") << "...witness form was " << wt << std::endl;
      }
      lp->addStep(lemmas[i], PfRule::MACRO_SR_PRED_INTRO, pfChildren, pfArgs);
    }
#endif
  }

  if (Debug.isOn("lemma-ites"))
  {
    Debug("lemma-ites") << "removed ITEs from lemma: " << ppNode << endl;
    Debug("lemma-ites") << " + now have the following " << lemmas.size()
                        << " lemma(s):" << endl;
    for (std::vector<Node>::const_iterator i = lemmas.begin();
         i != lemmas.end();
         ++i)
    {
      Debug("lemma-ites") << " + " << *i << endl;
    }
    Debug("lemma-ites") << endl;
  }

  // now, rewrite the lemmas
  Node retLemma;
  for (size_t i = 0, lsize = lemmas.size(); i < lsize; ++i)
  {
    Node rewritten = Rewriter::rewrite(lemmas[i]);
    if (lp != nullptr)
    {
      if (!CDProof::isSame(rewritten, lemmas[i]))
      {
        std::vector<Node> pfChildren;
        pfChildren.push_back(lemmas[i]);
        std::vector<Node> pfArgs;
        pfArgs.push_back(rewritten);
        lp->addStep(
            rewritten, PfRule::MACRO_SR_PRED_TRANSFORM, pfChildren, pfArgs);
      }
    }
    lemmas.replace(i, rewritten);
  }
}

struct preprocess_stack_element
{
  TNode node;
  bool children_added;
  preprocess_stack_element(TNode n) : node(n), children_added(false) {}
};

Node TheoryPreprocessor::theoryPreprocess(TNode assertion, LazyCDProof* lp)
{
  std::shared_ptr<TConvProofGenerator> tg;
  if (lp != nullptr)
  {
    // TODO: make the proof generator
  }
  Trace("theory::preprocess")
      << "TheoryPreprocessor::theoryPreprocess(" << assertion << ")" << endl;
  // spendResource();

  // Do a topological sort of the subexpressions and substitute them
  vector<preprocess_stack_element> toVisit;
  toVisit.push_back(assertion);

  while (!toVisit.empty())
  {
    // The current node we are processing
    preprocess_stack_element& stackHead = toVisit.back();
    TNode current = stackHead.node;

    Debug("theory::internal")
        << "TheoryPreprocessor::theoryPreprocess(" << assertion
        << "): processing " << current << endl;

    // If node already in the cache we're done, pop from the stack
    NodeMap::iterator find = d_ppCache.find(current);
    if (find != d_ppCache.end())
    {
      toVisit.pop_back();
      continue;
    }

    if (!d_logicInfo.isTheoryEnabled(Theory::theoryOf(current))
        && Theory::theoryOf(current) != THEORY_SAT_SOLVER)
    {
      stringstream ss;
      ss << "The logic was specified as " << d_logicInfo.getLogicString()
         << ", which doesn't include " << Theory::theoryOf(current)
         << ", but got a preprocessing-time fact for that theory." << endl
         << "The fact:" << endl
         << current;
      throw LogicException(ss.str());
    }

    // If this is an atom, we preprocess its terms with the theory ppRewriter
    if (Theory::theoryOf(current) != THEORY_BOOL)
    {
      Node ppRewritten = ppTheoryRewrite(current, tg.get());
      d_ppCache[current] = ppRewritten;
      Assert(Rewriter::rewrite(d_ppCache[current]) == d_ppCache[current]);
      continue;
    }

    // Not yet substituted, so process
    if (stackHead.children_added)
    {
      // Children have been processed, so substitute
      NodeBuilder<> builder(current.getKind());
      if (current.getMetaKind() == kind::metakind::PARAMETERIZED)
      {
        builder << current.getOperator();
      }
      for (unsigned i = 0; i < current.getNumChildren(); ++i)
      {
        Assert(d_ppCache.find(current[i]) != d_ppCache.end());
        builder << d_ppCache[current[i]];
      }
      // Mark the substitution and continue
      Node result = builder;
      if (result != current)
      {
        result = Rewriter::rewrite(result);
      }
      Debug("theory::internal")
          << "TheoryPreprocessor::theoryPreprocess(" << assertion
          << "): setting " << current << " -> " << result << endl;
      d_ppCache[current] = result;
      toVisit.pop_back();
    }
    else
    {
      // Mark that we have added the children if any
      if (current.getNumChildren() > 0)
      {
        stackHead.children_added = true;
        // We need to add the children
        for (TNode::iterator child_it = current.begin();
             child_it != current.end();
             ++child_it)
        {
          TNode childNode = *child_it;
          NodeMap::iterator childFind = d_ppCache.find(childNode);
          if (childFind == d_ppCache.end())
          {
            toVisit.push_back(childNode);
          }
        }
      }
      else
      {
        // No children, so we're done
        Debug("substitution::internal")
            << "SubstitutionMap::internalSubstitute(" << assertion
            << "): setting " << current << " -> " << current << endl;
        d_ppCache[current] = current;
        toVisit.pop_back();
      }
    }
  }
  if (lp != nullptr)
  {
    // TODO: proof generator makes proof here
  }

  // Return the substituted version
  return d_ppCache[assertion];
}

// Recursively traverse a term and call the theory rewriter on its sub-terms
Node TheoryPreprocessor::ppTheoryRewrite(TNode term, TConvProofGenerator* tg)
{
  NodeMap::iterator find = d_ppCache.find(term);
  if (find != d_ppCache.end())
  {
    return (*find).second;
  }
  unsigned nc = term.getNumChildren();
  if (nc == 0)
  {
    return d_engine.theoryOf(term)->ppRewrite(term, tg);
  }
  Trace("theory-pp") << "ppTheoryRewrite { " << term << endl;

  Node newTerm;
  // do not rewrite inside quantifiers
  if (term.isClosure())
  {
    newTerm = Rewriter::rewrite(term);
  }
  else
  {
    NodeBuilder<> newNode(term.getKind());
    if (term.getMetaKind() == kind::metakind::PARAMETERIZED)
    {
      newNode << term.getOperator();
    }
    unsigned i;
    for (i = 0; i < nc; ++i)
    {
      newNode << ppTheoryRewrite(term[i], tg);
    }
    newTerm = Rewriter::rewrite(Node(newNode));
  }
  Node newTerm2 = d_engine.theoryOf(newTerm)->ppRewrite(newTerm, tg);
  if (newTerm != newTerm2)
  {
    newTerm = ppTheoryRewrite(Rewriter::rewrite(newTerm2), tg);
  }
  d_ppCache[term] = newTerm;
  Trace("theory-pp") << "ppTheoryRewrite returning " << newTerm << "}" << endl;
  return newTerm;
}

}  // namespace theory
}  // namespace CVC4
