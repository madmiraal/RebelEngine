// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "hinge_joint_bullet.h"

#include "bullet_types_converter.h"
#include "bullet_utilities.h"
#include "rigid_body_bullet.h"

#include <BulletDynamics/ConstraintSolver/btHingeConstraint.h>

HingeJointBullet::HingeJointBullet(
    RigidBodyBullet* rbA,
    RigidBodyBullet* rbB,
    const Transform& frameA,
    const Transform& frameB
) :
    JointBullet() {
    Transform scaled_AFrame(frameA.scaled(rbA->get_body_scale()));
    scaled_AFrame.basis.rotref_posscale_decomposition(scaled_AFrame.basis);

    btTransform btFrameA;
    R_TO_B(scaled_AFrame, btFrameA);

    if (rbB) {
        Transform scaled_BFrame(frameB.scaled(rbB->get_body_scale()));
        scaled_BFrame.basis.rotref_posscale_decomposition(scaled_BFrame.basis);

        btTransform btFrameB;
        R_TO_B(scaled_BFrame, btFrameB);

        hingeConstraint = bulletnew(btHingeConstraint(
            *rbA->get_bt_rigid_body(),
            *rbB->get_bt_rigid_body(),
            btFrameA,
            btFrameB
        ));
    } else {
        hingeConstraint =
            bulletnew(btHingeConstraint(*rbA->get_bt_rigid_body(), btFrameA));
    }

    setup(hingeConstraint);
}

HingeJointBullet::HingeJointBullet(
    RigidBodyBullet* rbA,
    RigidBodyBullet* rbB,
    const Vector3& pivotInA,
    const Vector3& pivotInB,
    const Vector3& axisInA,
    const Vector3& axisInB
) :
    JointBullet() {
    btVector3 btPivotA;
    btVector3 btAxisA;
    R_TO_B(pivotInA * rbA->get_body_scale(), btPivotA);
    R_TO_B(axisInA * rbA->get_body_scale(), btAxisA);

    if (rbB) {
        btVector3 btPivotB;
        btVector3 btAxisB;
        R_TO_B(pivotInB * rbB->get_body_scale(), btPivotB);
        R_TO_B(axisInB * rbB->get_body_scale(), btAxisB);

        hingeConstraint = bulletnew(btHingeConstraint(
            *rbA->get_bt_rigid_body(),
            *rbB->get_bt_rigid_body(),
            btPivotA,
            btPivotB,
            btAxisA,
            btAxisB
        ));
    } else {
        hingeConstraint = bulletnew(
            btHingeConstraint(*rbA->get_bt_rigid_body(), btPivotA, btAxisA)
        );
    }

    setup(hingeConstraint);
}

real_t HingeJointBullet::get_hinge_angle() {
    return hingeConstraint->getHingeAngle();
}

void HingeJointBullet::set_param(
    PhysicsServer::HingeJointParam p_param,
    real_t p_value
) {
    switch (p_param) {
        case PhysicsServer::HINGE_JOINT_LIMIT_UPPER:
            hingeConstraint->setLimit(
                hingeConstraint->getLowerLimit(),
                p_value,
                hingeConstraint->getLimitSoftness(),
                hingeConstraint->getLimitBiasFactor(),
                hingeConstraint->getLimitRelaxationFactor()
            );
            break;
        case PhysicsServer::HINGE_JOINT_LIMIT_LOWER:
            hingeConstraint->setLimit(
                p_value,
                hingeConstraint->getUpperLimit(),
                hingeConstraint->getLimitSoftness(),
                hingeConstraint->getLimitBiasFactor(),
                hingeConstraint->getLimitRelaxationFactor()
            );
            break;
        case PhysicsServer::HINGE_JOINT_LIMIT_BIAS:
            hingeConstraint->setLimit(
                hingeConstraint->getLowerLimit(),
                hingeConstraint->getUpperLimit(),
                hingeConstraint->getLimitSoftness(),
                p_value,
                hingeConstraint->getLimitRelaxationFactor()
            );
            break;
        case PhysicsServer::HINGE_JOINT_LIMIT_SOFTNESS:
            hingeConstraint->setLimit(
                hingeConstraint->getLowerLimit(),
                hingeConstraint->getUpperLimit(),
                p_value,
                hingeConstraint->getLimitBiasFactor(),
                hingeConstraint->getLimitRelaxationFactor()
            );
            break;
        case PhysicsServer::HINGE_JOINT_LIMIT_RELAXATION:
            hingeConstraint->setLimit(
                hingeConstraint->getLowerLimit(),
                hingeConstraint->getUpperLimit(),
                hingeConstraint->getLimitSoftness(),
                hingeConstraint->getLimitBiasFactor(),
                p_value
            );
            break;
        case PhysicsServer::HINGE_JOINT_MOTOR_TARGET_VELOCITY:
            hingeConstraint->setMotorTargetVelocity(p_value);
            break;
        case PhysicsServer::HINGE_JOINT_MOTOR_MAX_IMPULSE:
            hingeConstraint->setMaxMotorImpulse(p_value);
            break;
        default:
            WARN_DEPRECATED_MSG(
                "The HingeJoint parameter " + itos(p_param) + " is deprecated."
            );
            break;
    }
}

real_t HingeJointBullet::get_param(PhysicsServer::HingeJointParam p_param
) const {
    switch (p_param) {
        case PhysicsServer::HINGE_JOINT_BIAS:
            return 0;
            break;
        case PhysicsServer::HINGE_JOINT_LIMIT_UPPER:
            return hingeConstraint->getUpperLimit();
        case PhysicsServer::HINGE_JOINT_LIMIT_LOWER:
            return hingeConstraint->getLowerLimit();
        case PhysicsServer::HINGE_JOINT_LIMIT_BIAS:
            return hingeConstraint->getLimitBiasFactor();
        case PhysicsServer::HINGE_JOINT_LIMIT_SOFTNESS:
            return hingeConstraint->getLimitSoftness();
        case PhysicsServer::HINGE_JOINT_LIMIT_RELAXATION:
            return hingeConstraint->getLimitRelaxationFactor();
        case PhysicsServer::HINGE_JOINT_MOTOR_TARGET_VELOCITY:
            return hingeConstraint->getMotorTargetVelocity();
        case PhysicsServer::HINGE_JOINT_MOTOR_MAX_IMPULSE:
            return hingeConstraint->getMaxMotorImpulse();
        default:
            WARN_DEPRECATED_MSG(
                "The HingeJoint parameter " + itos(p_param) + " is deprecated."
            );
            return 0;
    }
}

void HingeJointBullet::set_flag(
    PhysicsServer::HingeJointFlag p_flag,
    bool p_value
) {
    switch (p_flag) {
        case PhysicsServer::HINGE_JOINT_FLAG_USE_LIMIT:
            if (!p_value) {
                hingeConstraint->setLimit(-Math_PI, Math_PI);
            }
            break;
        case PhysicsServer::HINGE_JOINT_FLAG_ENABLE_MOTOR:
            hingeConstraint->enableMotor(p_value);
            break;
        case PhysicsServer::HINGE_JOINT_FLAG_MAX:
            break; // Can't happen, but silences warning
    }
}

bool HingeJointBullet::get_flag(PhysicsServer::HingeJointFlag p_flag) const {
    switch (p_flag) {
        case PhysicsServer::HINGE_JOINT_FLAG_USE_LIMIT:
            return true;
        case PhysicsServer::HINGE_JOINT_FLAG_ENABLE_MOTOR:
            return hingeConstraint->getEnableAngularMotor();
        default:
            return false;
    }
}
