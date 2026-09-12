#include "../../common/graphics/cameraobstruction.h"

#include <iostream>

int main() {
  using CameraObstruction::foliageTexture;
  constexpr const char* foliage[]={
    "MOWOTREETOP01.TGA", "mowobush02.tga", "MOWOFERN01.TGA", "MOWOREED02.TGA",
    "NW_NATURE_LEAVE_01.TGA", "NW_NATURE_GRASS_01.TGA", "NW_NATURE_FARN_01.TGA",
    "DECAL_MISC_SPIDERWEB.TGA", "OWDISPIDERWEBHANG.TGA", "NW_SEQ_NATURE_SMALLBUSH_01.TGA",
    "NW_NATURE_BRANCH_01.TGA",
    "MOWOROOT01.TGA", "MOWOROOT02.TGA", "MOWOROOT03.TGA",
    "NW_NATURE_ROOT_01.TGA", "NW_NATURE_ROOTS_01.TGA", "textures/moworoot01-c.tex",
    "NW_NATURE_TREE_NEEDLE_02.TGA", "NW_NATURE_TREE_NEEDLE_03.TGA", "NW_SEQ_NATURE_PINE_01.TGA",
    "NW_MISC_SEEROSE_01.TGA", "NW_MISC_SEEROSE_BLUETE_ROT_01.TGA",
    "NW_MISC_SEEROSE_BLUETE_WEISS_01.TGA", "NW_MISC_SEEROSE_BLUETE_ROT_02.TGA",
    "NW_MISC_SEEROSE_BLUETE_WEISS_02.TGA", "NW_MISC_DUCKWEED_01.TGA",
    "textures\\NW_NATURE_PLANT_01.TGA", "textures/OW_NATURE_BUSH_03.TGA"
    };
  constexpr const char* unchanged[]={
    "", "MOWOBARK01.TGA", "MOWOBARKLIANA01.TGA", "MOWOBARREL01.TGA", "OWODFLGRASSMI.TGA",
    "NW_NATURE_WALDBODEN_TO_GRASS_TRANS_01.TGA", "OW_SURFACE_GRASS_01.TGA",
    "NW_SEQ_NATURE_ROOFLEAVE_01.TGA", "NW_NATURE_BARK_01.TGA", "HUM_BODY_NAKED0.TGA",
    "NW_NATURE_BARK_02.TGA", "NW_SEQ_NATURE_TREEBARK_01.TGA",
    "NW_NATURE_WATER_01.TGA", "NW_NATURE_ACKER_01.TGA",
    "NW_NATURE_DARKWOOD_BODEN_ROOT_01.TGA", "NW_NATURE_DARKWOOD_BODEN_ROOT_02.TGA",
    "ROOTNOOSE.TGA", "ROOTNOOSE_GROUND.TGA", "ROOTNOOSE_BURST.TGA",
    "ITMW_1H_SWORD_01.TGA", "LEAF_A0.TGA", "UNKNOWN_MOD_GRASS.TGA"
    };
  for(auto name:foliage)
    if(!foliageTexture(name)) { std::cerr << "Missed foliage: " << name; return 1; }
  for(auto name:unchanged)
    if(foliageTexture(name)) { std::cerr << "Unexpected fading: " << name; return 1; }
  }
