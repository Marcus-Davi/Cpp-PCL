#pragma once

#include <pcl/registration/transformation_estimation.h>
#include <pcl/registration/warp_point_rigid_6d.h>
#include <pcl/console/print.h>

//idea for Feature-ICP usage;
// icp.setSourceEdges
// icp.setTargetEdges
// icp.setSourcePlanes
// icp.setTargetPlanes
// icp.setTransformationEstimation(edge p2l + plane p2p)

// mt trabalho!

using namespace pcl::registration;

template <typename PointSource, typename PointTarget, typename MatScalar = float>
class MyTransform : public TransformationEstimation<PointSource, PointTarget, MatScalar>
{
public:
    using Ptr = pcl::shared_ptr<MyTransform<PointSource, PointTarget, MatScalar>>;
    using ConstPtr = pcl::shared_ptr<const MyTransform<PointSource, PointTarget, MatScalar>>;
    using VectorX = Eigen::Matrix<MatScalar, Eigen::Dynamic, 1>;
    using MatrixX = Eigen::Matrix<MatScalar, Eigen::Dynamic, Eigen::Dynamic>;
    using Vector3 = Eigen::Matrix<MatScalar, 3, 1>;
    using Vector4 = Eigen::Matrix<MatScalar, 4, 1>;
    using Matrix3 = Eigen::Matrix<MatScalar, 3, 3>;
    using Matrix4 = typename TransformationEstimation<PointSource, PointTarget, MatScalar>::Matrix4;

    MyTransform()
    {
        warp_point_.reset(new WarpPointRigid6D<PointSource, PointTarget, MatScalar>);
    }
    ~MyTransform() {}

    inline void
    estimateRigidTransformation(
        const pcl::PointCloud<PointSource> &cloud_src,
        const pcl::PointCloud<PointTarget> &cloud_tgt,
        Matrix4 &transformation_matrix) const override
    {
        PCL_INFO("call 1");
    }

    void
    estimateRigidTransformation(
        const pcl::PointCloud<PointSource> &cloud_src,
        const std::vector<int> &indices_src,
        const pcl::PointCloud<PointTarget> &cloud_tgt,
        Matrix4 &transformation_matrix) const override
    {
        PCL_INFO("call 2");
    }

    void
    estimateRigidTransformation(
        const pcl::PointCloud<PointSource> &cloud_src,
        const std::vector<int> &indices_src,
        const pcl::PointCloud<PointTarget> &cloud_tgt,
        const std::vector<int> &indices_tgt,
        Matrix4 &transformation_matrix) const override
    {
        PCL_INFO("call 3");
    }

    // Gauss newton
    void estimateRigidTransformation(
        const pcl::PointCloud<PointSource> &cloud_src,
        const pcl::PointCloud<PointTarget> &cloud_tgt,
        const pcl::Correspondences &correspondences,
        Matrix4 &transformation_matrix) const
    {
        size_t n_pts = correspondences.size();
        PCL_DEBUG("Correspondences: %d\n", n_pts);
        // Parameters : tx, ty, tz, ax, ay, az
        VectorX parameters(6);

        Eigen::MatrixXf Jacobian(n_pts, 6); // 1 x 6
        MatrixX Hessian(6, 6);              // 6 x 3 x 3 x 6 -> 6 x 6

        VectorX NError(n_pts);
        VectorX Residuals(6); // 6 x 1

        // Rotations
        Matrix3 Rx;
        Matrix3 Ry;
        Matrix3 Rz;

        // Derivatives
        Matrix3 Rx_;
        Matrix3 Ry_;
        Matrix3 Rz_;

        // Compositions
        Matrix3 Rxyz;
        Matrix3 Rx_yz;
        Matrix3 Rxy_z;
        Matrix3 Rxyz_;

        parameters.setConstant(6, 0); // Init
        Hessian.setZero();
        Residuals.setZero();
        for (int j = 0; j < 3; ++j)
        {

            Vector3 translation(parameters(0), parameters(1), parameters(2));

            MatScalar c_alpha = std::cos(parameters(3));
            MatScalar s_alpha = std::sin(parameters(3));

            MatScalar c_beta = std::cos(parameters(4));
            MatScalar s_beta = std::sin(parameters(4));

            MatScalar c_gamma = std::cos(parameters(5));
            MatScalar s_gamma = std::sin(parameters(5));

            Rx << 1, 0, 0,
                0, c_alpha, -s_alpha,
                0, s_alpha, c_alpha;

            Ry << c_beta, 0, s_beta,
                0, 1, 0,
                -s_beta, 0, c_beta;

            Rz << c_gamma, -s_gamma, 0,
                s_gamma, c_gamma, 0,
                0, 0, 1;

            Rx_ << 0, 0, 0,
                0, -s_alpha, -c_alpha,
                0, c_alpha, -s_alpha;

            Ry_ << -s_beta, 0, c_beta,
                0, 0, 0,
                -c_beta, 0, -s_beta;

            Rz_ << -s_gamma, -c_gamma, 0,
                c_gamma, -s_gamma, 0,
                0, 0, 0;

            Rxyz = Rz * Ry * Rx;
            Rx_yz = Rz * Ry * Rx_;
            Rxy_z = Rz * Ry_ * Rx;
            Rxyz_ = Rz_ * Ry * Rx;

            for (int i = 0; i < n_pts; i++)
            {
                const int src_index = correspondences[i].index_query;
                const int tgt_index = correspondences[i].index_match;

                const PointSource &src_pt = cloud_src.points[src_index];
                const PointSource &tgt_pt = cloud_tgt.points[tgt_index];
                //  compute jacobian

                VectorX Error = Rxyz * src_pt.getVector3fMap() + translation - tgt_pt.getVector3fMap(); //p2p
                MatScalar error = Error.norm();

                Vector3 jac_alpha = Rx_yz * src_pt.getVector3fMap();
                Vector3 jac_beta = Rxy_z * src_pt.getVector3fMap();
                Vector3 jac_gamma = Rxyz_ * src_pt.getVector3fMap();

                // Jacobian(0, 0) = Error[0]; // 2 * e1
                // Jacobian(0, 1) = Error[1]; // 2 * e2
                // Jacobian(0, 2) = Error[2]; // 2 * e3

                // Jacobian(0, 3) = Error[0] * jac_alpha[0] + Error[1] * jac_alpha[1] + Error[2] * jac_alpha[2]; // alpha
                // Jacobian(0, 4) = Error[0] * jac_beta[0] + Error[1] * jac_beta[1] + Error[2] * jac_beta[2];    // beta
                // Jacobian(0, 5) = Error[0] * jac_gamma[0] + Error[1] * jac_gamma[1] + Error[2] * jac_gamma[2]; // gamma

                // Stack jacobians
                Jacobian(i, 0) = Error[0] / error; // 2 * e1
                Jacobian(i, 1) = Error[1] / error; // 2 * e2
                Jacobian(i, 2) = Error[2] / error; // 2 * e3

                Jacobian(i, 3) = (Error[0] * jac_alpha[0] + Error[1] * jac_alpha[1] + Error[2] * jac_alpha[2]) / error; // alpha
                Jacobian(i, 4) = (Error[0] * jac_beta[0] + Error[1] * jac_beta[1] + Error[2] * jac_beta[2]) / error;    // beta
                Jacobian(i, 5) = (Error[0] * jac_gamma[0] + Error[1] * jac_gamma[1] + Error[2] * jac_gamma[2]) / error; // gamma
                NError[i] = error;

                // Jacobian = Jacobian / error;

                // Hessian += Jacobian.transpose() * Jacobian; // 6 x 6 0.006197
                // Residuals += Jacobian.transpose() * error;
            }
            MatScalar damping = 1;
            Hessian = Jacobian.transpose() * Jacobian; // Fitness: 0.006057
            MatrixX diagonal = damping * Hessian.diagonal().asDiagonal();
            Hessian = Hessian + diagonal;
            Residuals = Jacobian.transpose() * NError;

            parameters -= Hessian.inverse() * Residuals;

        } // Gauss Newton Iteration for

        PCL_DEBUG("GN Solver : %f %f %f %f %f %f\n", parameters[0], parameters[1], parameters[2], parameters[3], parameters[4], parameters[5]);
        warp_point_->setParam(parameters);
        transformation_matrix = warp_point_->getTransform();
    }

protected:
    typename WarpPointRigid<PointSource, PointTarget, MatScalar>::Ptr warp_point_;
    const MyTransform *estimator_;

public:
    PCL_MAKE_ALIGNED_OPERATOR_NEW
};

// error[0] = (Rx * Ry * Rz * src_pt.getVector3fMap() + translation - tgt_pt.getVector3fMap()).dot(tgt_pt.getNormalVector3fMap()); //p2p
// Jacobian_.block<3, 3>(0, 0) = Eigen::MatrixXf::Identity(3, 3);
// Jacobian_.block<3, 1>(0, 3) = Rx_ * Ry * Rz * src_pt.getVector3fMap();
// Jacobian_.block<3, 1>(0, 4) = Rx * Ry_ * Rz * src_pt.getVector3fMap();
// Jacobian_.block<3, 1>(0, 5) = Rx * Ry * Rz_ * src_pt.getVector3fMap();

// Jacobian = Jacobian_.block<1, 6>(0, 0) * tgt_pt.normal_x +
//            Jacobian_.block<1, 6>(1, 0) * tgt_pt.normal_y +
//            Jacobian_.block<1, 6>(2, 0) * tgt_pt.normal_z;