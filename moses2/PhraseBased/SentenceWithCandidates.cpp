/*
 * SentenceWithCandidates.cpp
 *
 *  Created on: 14 Dec 2015
 *      Author: hieu
 */
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/algorithm/string.hpp> 

#include "SentenceWithCandidates.h"
#include "../System.h"
#include "../parameters/AllOptions.h"
#include "../legacy/Util2.h"
#include <unordered_map>

using namespace std;

namespace Moses2
{

SentenceWithCandidates *SentenceWithCandidates::CreateFromString(MemPool &pool, FactorCollection &vocab,
                                     const System &system, const std::string &str)
{
  SentenceWithCandidates *ret;
  
  // unordered_map<string,unordered_map<string, float>> ;

  // unordered_map<string, float> s;
  // s["abc"]=0.2;
  // s["awc"]=0.4;
  // s["abe"]=0.3;
  // translation_candidates["src_1"]=s; 

  // s.clear();
  // s["pqr"]=0.2;
  // s["yen"]=0.4;
  // s["dkg"]=0.5;
  // translation_candidates["src_2"]=s;  

  vector<string> input_parts; 
  boost::split(input_parts, str, boost::is_any_of("|||")); 

  if (input_parts.size()!=2){
    exit(1);
  }

  const string partstr = input_parts[0];
  // parseCandidates(input_parts[1]);
  
  if (system.options.input.xml_policy) {
    // xml
    ret = CreateFromStringXML(pool, vocab, system, partstr);
  } else {
    // no xml
    //cerr << "PB SentenceWithCandidates" << endl;
    std::vector<std::string> toks = Tokenize(partstr);

    size_t size = toks.size();
    ret = new (pool.Allocate<SentenceWithCandidates>()) SentenceWithCandidates(pool, size);
    ret->PhraseImplTemplate<Word>::CreateFromString(vocab, system, toks, false);
  }

  //cerr << "REORDERING CONSTRAINTS:" << ret->GetReorderingConstraint() << endl;
  //cerr << "ret=" << ret->Debug(system) << endl;

  return ret;
}

SentenceWithCandidates *SentenceWithCandidates::CreateFromStringXML(MemPool &pool, FactorCollection &vocab,
                                        const System &system, const std::string &str)
{
  SentenceWithCandidates *ret;

  vector<XMLOption*> xmlOptions;
  pugi::xml_document doc;

  string str2 = "<xml>" + str + "</xml>";
  pugi::xml_parse_result result = doc.load(str2.c_str(),
                                  pugi::parse_cdata | pugi::parse_wconv_attribute | pugi::parse_eol | pugi::parse_comments);
  pugi::xml_node topNode = doc.child("xml");

  std::vector<std::string> toks;
  XMLParse(pool, system, 0, topNode, toks, xmlOptions);

  // debug
  /*
  cerr << "xmloptions:" << endl;
  for (size_t i = 0; i < xmlOptions.size(); ++i) {
    cerr << xmlOptions[i]->Debug(system) << endl;
  }
  */

  // create words
  size_t size = toks.size();
  ret = new (pool.Allocate<SentenceWithCandidates>()) SentenceWithCandidates(pool, size);
  ret->PhraseImplTemplate<Word>::CreateFromString(vocab, system, toks, false);

  // xml
  ret->Init(system, size, system.options.reordering.max_distortion);

  ReorderingConstraint &reorderingConstraint = ret->GetReorderingConstraint();

  // set reordering walls, if "-monotone-at-punction" is set
  if (system.options.reordering.monotone_at_punct && ret->GetSize()) {
    reorderingConstraint.SetMonotoneAtPunctuation(*ret);
  }

  // set walls obtained from xml
  for(size_t i=0; i<xmlOptions.size(); i++) {
    const XMLOption *xmlOption = xmlOptions[i];
    if(strcmp(xmlOption->GetNodeName(), "wall") == 0) {
      if (xmlOption->startPos) {
        UTIL_THROW_IF2(xmlOption->startPos > ret->GetSize(), "wall is beyond the SentenceWithCandidates"); // no buggy walls, please
        reorderingConstraint.SetWall(xmlOption->startPos - 1, true);
      }
    } else if (strcmp(xmlOption->GetNodeName(), "zone") == 0) {
      reorderingConstraint.SetZone( xmlOption->startPos, xmlOption->startPos + xmlOption->phraseSize -1 );
    } else if (strcmp(xmlOption->GetNodeName(), "ne") == 0) {
      FactorType placeholderFactor = system.options.input.placeholder_factor;
      UTIL_THROW_IF2(placeholderFactor == NOT_FOUND,
                     "Placeholder XML in input. Must have argument -placeholder-factor [NUM]");
      UTIL_THROW_IF2(xmlOption->phraseSize != 1,
                     "Placeholder must only cover 1 word");

      const Factor *factor = vocab.AddFactor(xmlOption->GetEntity(), system, false);
      (*ret)[xmlOption->startPos][placeholderFactor] = factor;
    } else {
      // default - forced translation. Add to class variable
      ret->AddXMLOption(system, xmlOption);
    }
  }
  reorderingConstraint.FinalizeWalls();

  return ret;
}

} /* namespace Moses2 */

