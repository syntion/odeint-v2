/*
 * TODO:
 *
 * * implement rosenbrock_error_checker
 * * implement rosenbrock_controller
 * * implement generic_controlled_stepper
 */

#include <iostream>
#include <fstream>
#include <cmath>

#include <boost/timer.hpp>

#include <boost/numeric/odeint/stepper/rosenbrock4.hpp>
#include <boost/numeric/odeint/stepper/rosenbrock4_controller.hpp>
#include <boost/numeric/odeint/stepper/rosenbrock4_dense_output.hpp>
#include <boost/numeric/odeint/stepper/generation.hpp>
#include <boost/numeric/odeint/integrate/integrate_adaptive.hpp>

#include <boost/numeric/odeint/stepper/error_checker_explicit.hpp>
#include <boost/numeric/odeint/stepper/generic_controlled_stepper.hpp>
#include <boost/numeric/odeint/stepper/generic_controlled_stepper_explicit.hpp>
#include <boost/numeric/odeint/stepper/controller/default_controller.hpp>
#include <boost/numeric/odeint/stepper/controller/pi_controller.hpp>
#include <boost/numeric/odeint/stepper/controller/stiff_controller.hpp>


#define tab "\t"

namespace boost {
namespace numeric {
namespace odeint {


template< class Value ,  class Algebra  , class Operations >
class error_checker
{
public:

    typedef Value value_type;
    typedef Algebra algebra_type;
    typedef Operations operations_type;


    error_checker(
            const value_type eps_abs = static_cast< value_type >( 1.0e-6 ) ,
            const value_type eps_rel = static_cast< value_type >( 1.0e-6 )
            )
    : m_eps_abs( eps_abs ) , m_eps_rel( eps_rel )
    { }

    template< class State1 , class State2 , class Err , class Time >
    value_type error( const State1 &x_old , const State2 &x , Err &x_err , const Time &dt )
    {
        // this overwrites x_err !
        algebra.for_each3( x_err , x_old , x ,
                typename operations_type::template default_rel_error< value_type >( m_eps_abs , m_eps_rel ) );

        value_type res = algebra.reduce( x_err ,
                typename operations_type::template maximum< value_type >() , static_cast< value_type >( 0.0 ) );
        return res;
    }

private:

    value_type m_eps_abs;
    value_type m_eps_rel;
    algebra_type algebra;
};



template< class Value >
class rosenbrock_error_checker
{
public:

    typedef Value value_type;


    rosenbrock_error_checker(
            const value_type eps_abs = static_cast< value_type >( 1.0e-6 ) ,
            const value_type eps_rel = static_cast< value_type >( 1.0e-6 )
            )
    : m_eps_abs( eps_abs ) , m_eps_rel( eps_rel )
    { }

    template< class State1 , class State2 , class Err , class Time >
    value_type error( const State1 &x_old , const State2 &x , Err &x_err , const Time &dt )
    {
        value_type res = 0.0 , sk = 0.0;
        for( size_t i=0 ; i<x_old.size() ; ++i )
        {
            sk = m_eps_abs + m_eps_rel * std::max( std::abs( x_old[i] ) , std::abs( x[i] ) );
            res += x_err[i] * x_err[i] / sk / sk;
        }
        return sqrt( res / value_type( x_old.size() ) );
    }

private:

    value_type m_eps_abs;
    value_type m_eps_rel;
};





/*
 * ToDo: move this class into rosenbrock4_controller.hpp
 */
template< class Value >
class rosenbrock4_controller_tmp : public generic_controlled_stepper<
    rosenbrock4< double > ,
    rosenbrock_error_checker< double > ,
    stiff_controller< double > ,
    initially_resizer ,
    error_stepper_tag >
{
};







}
}
}



typedef boost::numeric::odeint::rosenbrock4< double > stepper_type;
typedef stepper_type::state_type state_type;
typedef stepper_type::matrix_type matrix_type;

const double mu = 1000.0;


struct vdp_stiff
{
    template< class State , class Deriv >
    void operator()( const State &x , Deriv &dxdt , double t )
    {
        dxdt[0] = x[1];
        dxdt[1] = -x[0] - mu * x[1] * (x[0]*x[0]-1.0);
    }
};

struct vdp_stiff_jacobi
{
    void operator()( const state_type &x , matrix_type &J , const double &t , state_type &dfdt )
    {
        J(0, 0) = 0.0;
        J(0, 1) = 1.0;
        J(1, 0) = -1.0 - 2.0*mu * x[0] * x[1];
        J(1, 1) = -mu * ( x[0] * x[0] - 1.0);

        dfdt[0] = 0.0;
        dfdt[1] = 0.0;
    }
};

struct streaming_observer
{
    std::ostream& m_out;

    streaming_observer( std::ostream &out ) : m_out( out ) { }

    template< class State >
    void operator()( const State &x , double t ) const
    {
        m_out << t;
        for( size_t i=0 ; i<x.size() ; ++i ) m_out << "\t" << x[i];
        m_out << "\n";
    }
};




int main( int argc , char **argv )
{
    using namespace std;
    using namespace boost::numeric::odeint;

    stepper_type stepper1;
    typedef rosenbrock4_controller_tmp< double > stepper2_type;

    state_type x1( 2 );
    x1[ 0 ] = 1.0 ; x1[ 1 ] = 1.0;
    state_type x2 = x1;
    state_type x3 = x1;
    state_type x4 = x1;

    boost::timer timer;
    const double t_max = 100000.0;

    ofstream fout1( "vdp_rb_1.dat" );
    timer.restart();
    size_t steps1 = integrate_adaptive( make_controlled( 1.0e-6 , 1.0e-6 , stepper1 ) ,
            make_pair( vdp_stiff() , vdp_stiff_jacobi() ) , x1 , 0.0 , t_max , 0.1 , streaming_observer( fout1 ) );
    double t1 = timer.elapsed();
    cout << steps1 << tab << t1 << tab << x1[0] << tab << x1[1] << endl;


    ofstream fout2( "vdp_rb_2.dat" );
    timer.restart();
    size_t steps2 = integrate_adaptive( stepper2_type() ,
            make_pair( vdp_stiff() , vdp_stiff_jacobi() ) , x2 , 0.0 , t_max , 0.1 , streaming_observer( fout2 ) );
    double t2 = timer.elapsed();
    cout << steps2 << tab << t2 << tab << x2[0] << tab << x2[1] << endl;






    return 0;
}
