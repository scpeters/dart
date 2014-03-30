#include "MyWorld.h"
#include "dart/utils/Paths.h"
#include "dart/utils/SkelParser.h"
#include <iostream>

using namespace Eigen;
using namespace dart::dynamics;

MyWorld::MyWorld() {

    // Load a skeleton from file
    mSkel = dart::utils::SkelParser::readSkeleton(DART_DATA_PATH"skel/temp.skel");
    mSkel->init();

}

MyWorld::~MyWorld() {
    delete mSkel;
}

// TODO: implement the IK solver.
void MyWorld::solve() {
}
