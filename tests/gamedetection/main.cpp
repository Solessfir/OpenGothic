#include "../../common/utils/gamedetection.h"

#include <iostream>
#include <stdexcept>

void check(bool ok,const char* message) {
  if(!ok) throw std::runtime_error(message);
  }

int main() {
  try {
    GameDetection::Evidence e;
    e.gothic1Score=1; // OU.BIN, without OU.DAT or the original Gothic.ini.
    e.gothic2World=true;
    e.addonWorld=true;
    auto v=GameDetection::detect(e);
    check(v.game==2 && v.patch==5 && v.hasZSStateLoop(),"Polish NotR package must select Gothic II addon behavior");
    e.gothic2Score=1; // English installation also contains OU.DAT.
    v=GameDetection::detect(e);
    check(v.game==2 && v.patch==5,"English NotR package must keep the same edition and patch");
    e.gothic1Score=2; // A folder named Gothic must not override the world contents.
    check(GameDetection::detect(e).game==2,"Installation folder names are only a fallback");
    e.hasPatch=true;
    e.patch=0;
    check(GameDetection::detect(e).patch==0,"An explicit classic patch setting must be preserved");
    e.patch=7;
    check(GameDetection::detect(e).patch==7,"An explicit patch version must be preserved");

    e={};
    e.gothic1Score=1;
    e.gothic2World=true;
    v=GameDetection::detect(e);
    check(v.game==2 && v.patch==0,"Classic Gothic II without an INI must not become Gothic I or NotR");

    e={};
    e.gothic1Score=1;
    e.gothic1World=true;
    v=GameDetection::detect(e);
    check(v.game==1 && v.patch==0 && !v.hasZSStateLoop(),"Gothic I remains supported without an INI");
    e.gothic2Score=3;
    check(GameDetection::detect(e).game==1,"Unambiguous Gothic I worlds override stale INI hints");

    e={};
    e.gothic1Score=1;
    check(GameDetection::detect(e).game==1,"Custom installations without standard worlds keep legacy detection");
    e.gothic2Score=2;
    check(GameDetection::detect(e).game==2,"Gothic II INI hints remain available to total conversions");
    e.gothic1World=true;
    e.gothic2World=true;
    check(GameDetection::detect(e).game==2,"Mixed world evidence retains the fallback scores");
    e.gothic1Score=3;
    check(GameDetection::detect(e).game==1,"Mixed world evidence does not force Gothic II");
    std::cout<<"Game detection tests passed\n";
    }
  catch(const std::exception& e) {
    std::cerr<<e.what()<<'\n';
    return 1;
    }
  }
