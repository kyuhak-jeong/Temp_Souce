#include "svmOpt.hpp"

sanOpt::sanOpt()
{
    m_verbose = 0; 
    m_max_it = 10000;
    m_init_lambda = 0.0001;
    m_up_factor = 5.0;
    m_down_factor = 5.0;
    m_target_derr = (1e-23);
}

double** sanOpt::createArray2D(int dim)
{
    double** array2D = (double**)new double* [dim] {nullptr,};

    for (int i = 0; i < dim; i++)
        array2D[i] = (double*)new double[dim] {0.0,};

    return array2D;
}

void sanOpt::setArray2D(int dim, double** array, double value)
{
    for (int i = 0; i < dim; i++)
        for (int j = 0; j < dim; j++)
            array[i][j] = value;
}

void sanOpt::setArray1D(int length, double* array, double value)
{
    for (int i = 0; i < length; i++)
            array[i] = value;
}


void sanOpt::deleteArray2D(int dim, double** array)
{
    for (int i = 0; i < dim; i++)
        delete[] array[i];

    delete[] array;
}

double sanOpt::error_func(double* par,		               /* [in] paramters */
                          double (*func)(double*, double), /* [in] target function */
                          void* fdata)					   /* [in] source data (domain) */
{
    int   idx = 0;
    double res = 0.0;
    double err = 0.0;
    vector<array<double, 2>>* pmt = (vector<array<double, 2>> *)fdata;

    for (idx = 0; idx < (int)pmt->size(); idx++)
    {
        res = (*pmt)[idx][0] - func(par, (*pmt)[idx][1]);  // target_value - estimated_value
        err += res * res;
    }

    return err;
}


void  sanOpt::levmarq_init()
{
    m_verbose = 0; /* for printing messages to debug mult and lamda values */
    m_max_it = 10000; /* the number of interation */
    m_init_lambda = 0.0001;
    m_up_factor = 5.0;
    m_down_factor = 5.0;
    m_target_derr = (1e-23);
}


int sanOpt::levmarq(int par_num,                                    /* number of parameters */
                    double* par,                                    /* array of parameters to be varied // output array, double results[9], k or d */
                    double(*func)(double*, double),                 /* function to be fit( b0+b1*r+ b2*r2 + b3*r3 + b4*r4 + b5*r5 + b6*r6 + b7*r7 + b8*r8) */
                    void (*gradient_func)(double*, double*, double),/* gradient of "func" with respect to the input parameters */
                    void* data)                                     /* pointer to any additional data required by the function, abs(fdata[i]) < 5 for all i */
{
    int   i, j, it, nit;
    int   ill_condition;
    int   index;
    double lambda, up, down, mult, weight;
    double err, newerr, delta_err, target_derr;
    double** Hessian = createArray2D(par_num);  // 2-order differential matrix(J^t * J)
    double** Cholesky = createArray2D(par_num); // Cholesky matrix == lower triangluar matrix
    double *g = new double [par_num] {0.0, };   // gradient
    double *d = new double [par_num] {0.0, };
    double *delta_par = new double [par_num] {0.0, };
    double* new_par = new double [par_num] {0.0, };
    double residual=0.0;
    vector<array<double, 2>> *pmt = (vector<array<double, 2>> *)data;

    nit = m_max_it;    /* maximum iteration number */
    lambda = m_init_lambda;
    up = m_up_factor;
    down = 1.0f / m_down_factor;
    target_derr = m_target_derr;

    weight = 1.0 / (double)(*pmt).size();
    delta_err = newerr = 0.0f; /* to avoid compiler warnings */

    /* calculate the initial error ("chi-squared") */
    err = error_func(par, func, data);  /* (y -func(fdata))^2 */

    /* main iteration */
    for (it = 0; it < nit; it++)
    {
        /* calculate the approximation to the Hessian and the "derivative" d */
        setArray1D(par_num, d, 0.0);
        setArray2D(par_num, Hessian, 0.0);

        for (index = 0; index < (int)(*pmt).size(); index++) /* as many as the number of measured data */
        {
            setArray1D(par_num, g, 0.0);
            gradient_func(g, par, (*pmt)[index][1]);
            residual = ((*pmt)[index][0] - func(par, (*pmt)[index][1]));

            for (i = 0; i < par_num; i++)  /* as many as the number of parameters */
            {
                d[i] += residual * g[i] * weight; // J^t * residual
                for (j = 0; j <= i; j++) /* get Hessian matrix from gradient */
                {
                    Hessian[i][j] += g[i] * g[j] * weight;  /* H = J^t * J  */
                }
            }
        }

        mult = 1.0f + lambda;
        ill_condition = 1;
        setArray1D(par_num, new_par, 0.0);
        setArray2D(par_num, Cholesky, 0.0);

        while (ill_condition && (it < nit))
        {
            for (i = 0; i < par_num; i++) /* diagonal components multiplied of Hessian by mult */
            {
                Hessian[i][i] = (Hessian[i][i] * mult);
            }

            ill_condition = cholesky_decomp(par_num, Cholesky, Hessian); /* Hessian matrix: symmetric matrix, LU decomposition,  */

            if (!ill_condition)
            {
                for (i = 0; i < par_num; i++) /* update parameters */
                    new_par[i] = par[i] + delta_par[i];

                newerr = error_func(new_par, func, data); /* get error value with new parameters */
                delta_err = newerr - err;
                ill_condition = (delta_err > 0.0f);
            }

            /*	  if (verbose)
                      printf("it = %4d,   lambda = %10g,   err = %10g,  derr = %10g\n", it, lambda, err, derr);
            */

            if (ill_condition) /* Hessian matrix is not symmetric positive definite */
            {
                mult = (1.0 + lambda * up) / (1.0 + lambda);
                lambda *= up;
                it++;
            }
        }

        for (i = 0; i < par_num; i++)
            par[i] = new_par[i];

        err = newerr;
        lambda *= down;

        if ((!ill_condition) && (-delta_err < target_derr))
            break;
    }

    m_final_it = it;
    m_final_err = err;
    m_final_derr = delta_err;

    delete[] g;
    delete[] d;
    delete[] delta_par;
    delete[] new_par;
    deleteArray2D(par_num, Hessian); 
    deleteArray2D(par_num, Cholesky); /* Cholesky matrix == lower triangluar matrix */

    return (it == nit);
}



int sanOpt::cholesky_decomp(int dim,    /* [in] dimension of a square matrix */
                            double **L, /* [out] Cholesky factor L (i.e, lower triangluar matrix) */
                            double **A) /* [in] symmetric positive definite matrix */
{
    double sum = 0.0;

    for (int i = 0; i < dim; i++)
    {
        for (int j = 0; j < i; j++)
        {
            sum = 0.0f;
            for (int k = 0; k < j; k++)
                sum += L[i][k] * L[j][k];

            L[i][j] = (A[i][j] - sum) / L[j][j];
        }

        sum = 0.0f;
        for (int k = 0; k < i; k++)
            sum += L[i][k] * L[i][k];

        sum = A[i][i] - sum;

        if (sum < TOL)
            return 1; /* matrix A is not positive-definite */

        L[i][i] = sqrt(sum);
    }

    return 0; /* succeeded, matrix A is symetric positive definite*/
}


void sanOpt::solve_axb_cholesky(int dim,	/* [in] dimension of a square matrix(etc, triangluar matrix) */
                                double** L, /* [in] Cholesky factor, L(i.e, lower triangle matrix) */
                                double* x,	/* [out] known vector and solution x in Ax = b */
                                double* b)	/* [in] right-side vector b in Ax = b */
{
    double sum = 0.0;

    /* solve L*y = b for y (where x[] is used to store y) */
    for (int i = 0; i < dim; i++)
    {
        sum = 0.0;
        for (int j = 0; j < i; j++)
            sum += L[i][j] * x[j];

        x[i] = (b[i] - sum) / L[i][i];
    }

    /* solve L^T*x = y for x (where x[] is used to store both y and x) */
    for (int i = (dim - 1); i >= 0; i--)
    {
        sum = 0.0;
        for (int j = i + 1; j < dim; j++)
            sum += L[j][i] * x[j];

        x[i] = (x[i] - sum) / L[i][i];
    }

}
