/*
 * Copyright (c) 2014, Georgia Tech Research Corporation
 * All rights reserved.
 *
 * Author(s): Karen Liu <karenliu@cc.gatech.edu>,
 *            Jeongseok Lee <jslee02@gmail.com>
 *
 * Geoorgia Tech Graphics Lab and Humanoid Robotics Lab
 *
 * Directed by Prof. C. Karen Liu and Prof. Mike Stilman
 * <karenliu@cc.gatech.edu> <mstilman@cc.gatech.edu>
 *
 * This file is provided under the following "BSD-style" License:
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *   CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 *   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 *   USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 *   AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *   ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *   POSSIBILITY OF SUCH DAMAGE.
 */

#include "dart/constraint/ContactConstraint.h"

#include <iostream>

#include "dart/dynamics/BodyNode.h"
#include "dart/dynamics/Skeleton.h"
#include "dart/lcpsolver/lcp.h"

#define DART_CONTACT_CONSTRAIN_EPSILON 1e-6

namespace dart {
namespace constraint {

//==============================================================================
ContactConstraint::ContactConstraint(const collision::Contact& _contact)
  : Constraint(CT_DYNAMIC),
    mFirstFrictionalDirection(Eigen::Vector3d::UnitZ()),
    _IsFrictionOn(true)
{
  mContacts.push_back(_contact);

  // TODO(JS):
  mBodyNode1 = _contact.collisionNode1->getBodyNode();
  mBodyNode2 = _contact.collisionNode2->getBodyNode();

  // TODO(JS): Assume the frictional coefficient can be changed during
  //           simulation steps.
  // Update mFrictionalCoff
  _frictionalCoff = std::min(mBodyNode1->getFrictionCoeff(),
                             mBodyNode2->getFrictionCoeff());
  if (_frictionalCoff > DART_FRICTION_THRESHOLD)
  {
    _IsFrictionOn = true;

    // Update frictional direction
    _updateFirstFrictionalDirection();
  }
  else
  {
    _IsFrictionOn = false;
  }

  // Compute local contact Jacobians expressed in body frame
  if (_IsFrictionOn)
  {
    // Set the dimension of this constraint. 1 is for Normal direction constraint.
    // TODO(JS): Assumed that the number of contact is not static.
    // TODO(JS): Adjust following code once use of mNumFrictionConeBases is
    //           implemented.
    //  mDim = mContacts.size() * (1 + mNumFrictionConeBases);
    mDim = mContacts.size() * 3;

    mJacobians1.resize(mDim);
    mJacobians2.resize(mDim);

    // Intermediate variables
    int idx = 0;

    Eigen::Vector3d bodyDirection1;
    Eigen::Vector3d bodyDirection2;

    Eigen::Vector3d bodyPoint1;
    Eigen::Vector3d bodyPoint2;

    for (int i = 0; i < mContacts.size(); ++i)
    {
      const collision::Contact& ct = mContacts[i];

      // TODO(JS): Assumed that the number of tangent basis is 2.
      Eigen::MatrixXd D = _getTangentBasisMatrixODE(ct.normal);

      // Jacobian for normal contact
      bodyDirection1.noalias()
          = mBodyNode1->getWorldTransform().linear().transpose() * ct.normal;
      bodyDirection2.noalias()
          = mBodyNode2->getWorldTransform().linear().transpose() * -ct.normal;

      bodyPoint1.noalias()
          = mBodyNode1->getWorldTransform().inverse() * ct.point;
      bodyPoint2.noalias()
          = mBodyNode2->getWorldTransform().inverse() * ct.point;

      mJacobians1[idx].head<3>().noalias() = bodyPoint1.cross(bodyDirection1);
      mJacobians2[idx].head<3>().noalias() = bodyPoint2.cross(bodyDirection2);

      mJacobians1[idx].tail<3>().noalias() = bodyDirection1;
      mJacobians2[idx].tail<3>().noalias() = bodyDirection2;

      ++idx;

      // Jacobian for directional friction 1
      bodyDirection1.noalias()
          = mBodyNode1->getWorldTransform().linear().transpose() * D.col(0);
      bodyDirection2.noalias()
          = mBodyNode2->getWorldTransform().linear().transpose() * -D.col(0);

      bodyPoint1.noalias()
          = mBodyNode1->getWorldTransform().inverse() * ct.point;
      bodyPoint2.noalias()
          = mBodyNode2->getWorldTransform().inverse() * ct.point;

      mJacobians1[idx].head<3>().noalias() = bodyPoint1.cross(bodyDirection1);
      mJacobians2[idx].head<3>().noalias() = bodyPoint2.cross(bodyDirection2);

      mJacobians1[idx].tail<3>().noalias() = bodyDirection1;
      mJacobians2[idx].tail<3>().noalias() = bodyDirection2;

      ++idx;

      // Jacobian for directional friction 2
      bodyDirection1.noalias()
          = mBodyNode1->getWorldTransform().linear().transpose() * D.col(1);
      bodyDirection2.noalias()
          = mBodyNode2->getWorldTransform().linear().transpose() * -D.col(1);

      bodyPoint1.noalias()
          = mBodyNode1->getWorldTransform().inverse() * ct.point;
      bodyPoint2.noalias()
          = mBodyNode2->getWorldTransform().inverse() * ct.point;

      mJacobians1[idx].head<3>().noalias() = bodyPoint1.cross(bodyDirection1);
      mJacobians2[idx].head<3>().noalias() = bodyPoint2.cross(bodyDirection2);

      mJacobians1[idx].tail<3>().noalias() = bodyDirection1;
      mJacobians2[idx].tail<3>().noalias() = bodyDirection2;

      ++idx;
    }
  }
  else
  {
    // Set the dimension of this constraint.
    mDim = mContacts.size();

    mJacobians1.resize(mDim);
    mJacobians2.resize(mDim);

    Eigen::Vector3d bodyDirection1;
    Eigen::Vector3d bodyDirection2;

    Eigen::Vector3d bodyPoint1;
    Eigen::Vector3d bodyPoint2;

    for (int i = 0; i < mContacts.size(); ++i)
    {
      const collision::Contact& ct = mContacts[i];

      bodyDirection1.noalias()
          = mBodyNode1->getWorldTransform().linear().transpose() * ct.normal;
      bodyDirection2.noalias()
          = mBodyNode2->getWorldTransform().linear().transpose() * -ct.normal;

      bodyPoint1.noalias()
          = mBodyNode1->getWorldTransform().inverse() * ct.point;
      bodyPoint2.noalias()
          = mBodyNode2->getWorldTransform().inverse() * ct.point;

      mJacobians1[i].head<3>().noalias() = bodyPoint1.cross(bodyDirection1);
      mJacobians2[i].head<3>().noalias() = bodyPoint2.cross(bodyDirection2);

      mJacobians1[i].tail<3>().noalias() = bodyDirection1;
      mJacobians2[i].tail<3>().noalias() = bodyDirection2;
    }
  }
}

//==============================================================================
ContactConstraint::~ContactConstraint()
{
}

//==============================================================================
void ContactConstraint::setFirstFrictionDir(
    const Eigen::Vector3d& _dir)
{
  mFirstFrictionalDirection = _dir.normalized();
}

//==============================================================================
const Eigen::Vector3d&ContactConstraint::getFirstFrictionlDir() const
{
  return mFirstFrictionalDirection;
}

//==============================================================================
void ContactConstraint::preprocess()
{
  std::cout << "ContactConstraintTEST::preprocess(): "
            << "Not implemented."
            << std::endl;
}

//==============================================================================
void ContactConstraint::update()
{
  std::cout << "ContactConstraintTEST::update(): "
            << "Not implemented."
            << std::endl;
}

//==============================================================================
void ContactConstraint::fillLcpOde(ODELcp* _lcp, int _idx)
{
  // Fill w, where the LCP form is Ax = b + w (x >= 0, w >= 0, x^T w = 0)
  _getRelVelocity(_lcp->b, _idx);

  //----------------------------------------------------------------------------
  // Friction case
  //----------------------------------------------------------------------------
  if (_IsFrictionOn)
  {
    for (int i = 0; i < mContacts.size(); ++i)
    {
      // Bias term, w, should be zero
      _lcp->w[_idx] = 0.0;
      _lcp->w[_idx + 1] = 0.0;
      _lcp->w[_idx + 2] = 0.0;

      // Upper and lower bounds of normal impulsive force
      _lcp->lb[_idx] = 0.0;
      _lcp->ub[_idx] = dInfinity;

      // Upper and lower bounds of tangential direction-1 impulsive force
      _lcp->lb[_idx + 1] = -_frictionalCoff;
      _lcp->ub[_idx + 1] =  _frictionalCoff;
      _lcp->frictionIndex[_idx + 1] = _idx;

      // Upper and lower bounds of tangential direction-2 impulsive force
      _lcp->lb[_idx + 2] = -_frictionalCoff;
      _lcp->ub[_idx + 2] =  _frictionalCoff;
      _lcp->frictionIndex[_idx + 2] = _idx;

      // TODO(JS): Penetration correction should be here

      // TODO(JS): Bounce condition should be here

      // TODO(JS): Initial guess

      // Increase index
      _idx += 3;
    }
  }
  //----------------------------------------------------------------------------
  // Frictionless case
  //----------------------------------------------------------------------------
  else
  {
    for (int i = 0; i < mContacts.size(); ++i)
    {
      // Bias term, w, should be zero
      _lcp->w[_idx] = 0.0;

      // Upper and lower bounds of normal impulsive force
      _lcp->lb[_idx] = 0.0;
      _lcp->ub[_idx] = dInfinity;

      // TODO(JS): Penetration correction should be here

      // TODO(JS): Bounce condition should be here

      // TODO(JS): Initial guess

      // Increase index
      _idx++;
    }
  }
}

//==============================================================================
void ContactConstraint::applyUnitImpulse(int _idx)
{
  assert(0 <= _idx && _idx < mDim && "Invalid Index.");

  // Self collision case
  if (mBodyNode1->getSkeleton() == mBodyNode2->getSkeleton()
      && (mBodyNode1->isImpulseReponsible()
          || mBodyNode2->isImpulseReponsible()))
  {
    mBodyNode1->getSkeleton()->clearImpulseTest();

    if (mBodyNode1->isImpulseReponsible())
      mBodyNode1->getSkeleton()->updateBiasImpulse(mBodyNode1,
                                                    mJacobians1[_idx]);

    if (mBodyNode2->isImpulseReponsible())
      mBodyNode2->getSkeleton()->updateBiasImpulse(mBodyNode2,
                                                    mJacobians2[_idx]);

    mBodyNode1->getSkeleton()->updateVelocityChange();
    return;
  }

  if (mBodyNode1->isImpulseReponsible())
  {
    mBodyNode1->getSkeleton()->clearImpulseTest();
    mBodyNode1->getSkeleton()->updateBiasImpulse(mBodyNode1, mJacobians1[_idx]);
    mBodyNode1->getSkeleton()->updateVelocityChange();
  }

  if (mBodyNode2->isImpulseReponsible())
  {
    mBodyNode2->getSkeleton()->clearImpulseTest();
    mBodyNode2->getSkeleton()->updateBiasImpulse(mBodyNode2, mJacobians2[_idx]);
    mBodyNode2->getSkeleton()->updateVelocityChange();
  }
}

//==============================================================================
void ContactConstraint::getDelVelocity(double* _delVel, int _idx)
{
  assert(_delVel != NULL && "Null pointer is not allowed.");

  for (int i = 0; i < mDim; ++i)
  {
    _delVel[i + _idx] = 0.0;

    if (mBodyNode1->getSkeleton()->isImpulseApplied()
        && mBodyNode1->isImpulseReponsible())
    {
      _delVel[i + _idx]
          += mJacobians1[i].dot(mBodyNode1->getBodyVelocityChange());
    }

    if (mBodyNode2->getSkeleton()->isImpulseApplied()
        && mBodyNode2->isImpulseReponsible())
    {
      _delVel[i + _idx]
          += mJacobians2[i].dot(mBodyNode2->getBodyVelocityChange());
    }
  }
}

//==============================================================================
void ContactConstraint::excite()
{
  if (mBodyNode1->isImpulseReponsible())
    mBodyNode1->getSkeleton()->setImpulseApplied(true);

  if (mBodyNode2->isImpulseReponsible())
    mBodyNode2->getSkeleton()->setImpulseApplied(true);
}

//==============================================================================
void ContactConstraint::unexcite()
{
  if (mBodyNode1->isImpulseReponsible())
    mBodyNode1->getSkeleton()->setImpulseApplied(false);

  if (mBodyNode2->isImpulseReponsible())
    mBodyNode2->getSkeleton()->setImpulseApplied(false);
}

//==============================================================================
void ContactConstraint::applyConstraintImpulse(double* _lambda, int _idx)
{
  //----------------------------------------------------------------------------
  // Friction case
  //----------------------------------------------------------------------------
  if (_IsFrictionOn)
  {
    for (int i = 0; i < mContacts.size(); ++i)
    {
      // Normal impulsive force
//      mContacts[i]->lambda[0] = _lambda[_idx];
      mBodyNode1->addConstraintImpulse(mJacobians1[i] * _lambda[_idx]);
      mBodyNode2->addConstraintImpulse(mJacobians2[i] * _lambda[_idx]);
      _idx++;

      // Tangential direction-1 impulsive force
//      mContacts[i]->lambda[1] = _lambda[_idx];
      mBodyNode1->addConstraintImpulse(mJacobians1[i] * _lambda[_idx]);
      mBodyNode2->addConstraintImpulse(mJacobians2[i] * _lambda[_idx]);
      _idx++;

      // Tangential direction-2 impulsive force
//      mContacts[i]->lambda[2] = _lambda[_idx];
      mBodyNode1->addConstraintImpulse(mJacobians1[i] * _lambda[_idx]);
      mBodyNode2->addConstraintImpulse(mJacobians2[i] * _lambda[_idx]);
      _idx++;
    }
  }
  //----------------------------------------------------------------------------
  // Frictionless case
  //----------------------------------------------------------------------------
  else
  {
    for (int i = 0; i < mContacts.size(); ++i)
    {
      // Normal impulsive force
//			pContactPts[i]->lambda[0] = _lambda[i];
      mBodyNode1->addConstraintImpulse(mJacobians1[i] * _lambda[_idx]);
      mBodyNode2->addConstraintImpulse(mJacobians2[i] * _lambda[_idx]);
      _idx++;
    }
  }
}

//==============================================================================
void ContactConstraint::_getRelVelocity(double* _relVel, int _idx)
{
  assert(_relVel != NULL && "Null pointer is not allowed.");

  for (int i = 0; i < mDim; ++i)
  {
    _relVel[i + _idx] = 0.0;

    if (mBodyNode1->isImpulseReponsible())
      _relVel[i + _idx] -= mJacobians1[i].dot(mBodyNode1->getBodyVelocity());

    if (mBodyNode2->isImpulseReponsible())
      _relVel[i + _idx] -= mJacobians2[i].dot(mBodyNode2->getBodyVelocity());
  }
}

//==============================================================================
bool ContactConstraint::isActive()
{
  std::cout << "ContactConstraintTEST::isActive(): "
            << "Not implemented."
            << std::endl;

  return true;
}

//==============================================================================
void ContactConstraint::_updateVelocityChange(int _idx)
{
  std::cout << "ContactConstraintTEST::_exciteSystem1And2(): "
            << "Not implemented."
            << std::endl;
}

//==============================================================================
void ContactConstraint::_updateFirstFrictionalDirection()
{
  std::cout << "ContactConstraintTEST::_updateFirstFrictionalDirection(): "
            << "Not finished implementation."
            << std::endl;

  // TODO(JS): Not implemented
  // Refer to:
  // https://github.com/erwincoumans/bullet3/blob/master/src/BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.cpp#L910
//  mFirstFrictionalDirection;
}

//==============================================================================
Eigen::MatrixXd ContactConstraint::_getTangentBasisMatrixODE(
    const Eigen::Vector3d& _n)
{
  // TODO(JS): Use mNumFrictionConeBases
  // Check if the number of bases is even number.
//  bool isEvenNumBases = mNumFrictionConeBases % 2 ? true : false;

  Eigen::MatrixXd T(Eigen::MatrixXd::Zero(3, 2));

  // Pick an arbitrary vector to take the cross product of (in this case,
  // Z-axis)
  Eigen::Vector3d tangent = mFirstFrictionalDirection.cross(_n);

  // TODO(JS): Modify following lines once _updateFirstFrictionalDirection() is
  //           implemented.
  // If they're too close, pick another tangent (use X-axis as arbitrary vector)
  if (tangent.norm() < DART_CONTACT_CONSTRAIN_EPSILON)
    tangent = Eigen::Vector3d::UnitX().cross(_n);

  tangent.normalize();

  // Rotate the tangent around the normal to compute bases.
  // Note: a possible speedup is in place for mNumDir % 2 = 0
  // Each basis and its opposite belong in the matrix, so we iterate half as
  // many times
  double angle = 0.5 * DART_PI;
  T.col(0) = tangent;
  T.col(1) = Eigen::Quaterniond(Eigen::AngleAxisd(angle, _n)) * tangent;
  return T;
}

}  // namespace constraint
}  // namespace dart
