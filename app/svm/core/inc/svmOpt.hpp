#ifndef SVMOPT_HPP_
#define SVMOPT_HPP_

#include "svmCore.hpp"

#define TOL (1e-30)

class sanOpt
{
public:
	int		m_verbose;
	int		m_max_it;   /* maximual number of iterations */
	double	m_init_lambda;
	double	m_up_factor;
	double	m_down_factor;
	double	m_target_derr;

	int		m_final_it;  /* the number of iterations */
	double	m_final_err;
	double	m_final_derr;

public:
	sanOpt();

	double** createArray2D(int dim);
	void setArray2D(int dim, double** array, double value);
	void setArray1D(int length, double* array, double value);
	void deleteArray2D(int dim, double** array);

	int cholesky_decomp(int dim, double** L, double **A);
	void solve_axb_cholesky(int dim, double **L, double* x, double* b);

	void levmarq_init();
	int levmarq(int npar, double* par, double(*func)(double*, double), void (*grad)(double*, double*, double),	void* data);
	double error_func(double* par, double (*func)(double*, double), void* fdata);
};

#endif
