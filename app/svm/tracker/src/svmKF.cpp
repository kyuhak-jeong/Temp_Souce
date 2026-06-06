#include "svmKF.h"


sanKF::sanKF(unsigned int num_states, unsigned int num_obs) : num_states_(num_states), num_obs_(num_obs) 
{
    /*** Predict ***/
    // State vector
    x_ = Eigen::VectorXd::Zero(num_states);

    // Predicted(a prior) state vector
    x_predict_ = Eigen::VectorXd::Zero(num_states);

    // State transition matrix F_
    F_ = Eigen::MatrixXd::Zero(num_states, num_states);

    // Error covariance matrix P
    P_ = Eigen::MatrixXd::Zero(num_states, num_states);

    // Predicted(a prior) error covariance matrix
    P_predict_ = Eigen::MatrixXd::Zero(num_states, num_states);

    // Covariance matrix of process noise
    Q_ = Eigen::MatrixXd::Zero(num_states, num_states);

    /*** Update ***/
    // Observation matrix
    H_ = Eigen::MatrixXd::Zero(num_obs, num_states);

    // Covariance matrix of observation noise
    R_ = Eigen::MatrixXd::Zero(num_obs, num_obs);

    log_likelihood_delta_ = 0.0;

    NIS_ = 0.0;
}


void sanKF::Coast() 
{
    x_predict_ = F_ * x_;
    P_predict_ = F_ * P_ * F_.transpose() + Q_;
}


void sanKF::Predict()
{
    Coast();

    x_ = x_predict_;
    P_ = P_predict_;
}


Eigen::VectorXd sanKF::PredictionToObservation(const Eigen::VectorXd &state)
{
    return (H_*state);
}


void sanKF::Update(const Eigen::VectorXd& z)
{
    Eigen::VectorXd z_predict = PredictionToObservation(x_predict_);

    // y - innovation, z - real observation, z_predict - predicted observation
    Eigen::VectorXd y = z - z_predict;

    Eigen::MatrixXd Ht = H_.transpose();

    // S - innovation covariance
    Eigen::MatrixXd S = H_ * P_predict_ * Ht + R_;

    NIS_ = float(y.transpose() * S.inverse() * y);

#if (0)
    std::cout << std::endl;
    std::cout << "P_predict = " << std::endl;
    std::cout << P_predict_ << std::endl;

    std::cout << "Z = " << std::endl;
    std::cout << z << std::endl;

    std::cout << "Z_pred = " << std::endl;
    std::cout << z_predict << std::endl;

    std::cout << "y = " << std::endl;
    std::cout << y << std::endl;

    std::cout << "S = " << std::endl;
    std::cout << S << std::endl;

    std::cout << "NIS = " << NIS_ << std::endl;
#endif

    // K - Kalman gain
    Eigen::MatrixXd K = P_predict_ * Ht * S.inverse();

    // Updated state estimation
    x_ = x_predict_ + K * y;

    Eigen::MatrixXd I = Eigen::MatrixXd::Identity(num_states_, num_states_);


#if (1)    
    P_ = (I - K * H_) * P_predict_ * (I - K * H_).transpose() + K * R_ * K.transpose(); // Joseph form
#else     
    P_ = (I - K * H_) * P_predict_; // Optimal gain
#endif
}


float sanKF::CalculateLogLikelihood(const Eigen::VectorXd& y, const Eigen::MatrixXd& S) 
{
    float log_likelihood = 0.0f;

    // Note: Computing log(M.determinant()) in Eigen C++ is risky for large matrices since it may overflow or underflow.
    // compute the Cholesky decomposition of the innovation covariance matrix, because it is symmetric
    // S = L * L^T = U^T * U
    // then retrieve factor L in the decomposition
    auto llt = S.llt(); 
    auto& L = llt.matrixL();

    // find log determinant of innovation covariance matrix
    float log_determinant = 0;
    for (unsigned int i = 0; i < S.rows(); i++)
        log_determinant += (float)log(L(i, i));  // changed by DGH

    log_determinant *= 2;

    // log-likelihood expression for current iteration
    log_likelihood = (float)( -0.5 * (y.transpose() * S.inverse() * y + num_obs_ * log(2.0 * M_PI) + log_determinant));

    if (std::isnan(log_likelihood)) 
    {
    	//log_likelihood = -1e50; 
        log_likelihood = -FLT_MAX;  // changed by DGH
	}

	return log_likelihood;
}
