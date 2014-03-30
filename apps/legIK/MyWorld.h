#ifndef _MYWORLD_
#define _MYWORLD_

#include <vector>
#include "dart/utils/SkelParser.h"
#include "dart/dynamics/Skeleton.h"
#include "dart/dynamics/BodyNode.h"
#include "dart/dynamics/Joint.h"
#include "dart/dynamics/Marker.h"


class MyWorld {
 public:
    MyWorld();
    virtual ~MyWorld();
    dart::dynamics::Skeleton* getSkel() {
        return mSkel;
    }

    // TODO: solve IK problem. You can change the function prototype as you see fit.
    void solve();

 protected:    
    dart::dynamics::Skeleton *mSkel;
};

#endif
