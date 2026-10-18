#pragma once

#include <cmath>
#include <functional>

namespace score::numerical_methods {

// Example root-finding function using the bisection method
inline double bisection(std::function<double(double)> f, double a, double b, double total = 1e-8) {
    double f_a = f(a), f_b = f(b);

    if (f_a * f_b > 0) throw std::invalid_argument("Function must have opposite signs at the endpoints a and b.");
    while ((b-a)/2 > total) {
        double c = (a + b) / 2;
        double f_c = f(c);
        if (f_c == 0) return c;
        if (f_a * f_c < 0) {
            b = c;
            f_b = f_c;
        } else {
            a = c;
            f_a = f_c;
        }
    }
    return (a + b) / 2;
}

std::function<double(double)> get_derivative(std::function<double(double)> f, double h = 1e-8) {
    return [f, h](double x) {
        return (f(x + h) - f(x - h)) / (2 * h);
    };
}


// newtons method
inline double newtons_method(std::function<double(double)> f, double x0, double total = 1e-8, int max_iter = 1000) {
    auto df = get_derivative(f);
    double x = x0;
    for (int i = 0; i < max_iter; ++i) {
        double f_x = f(x);
        double df_x = df(x);
        if (std::abs(f_x) < total) return x;
        if (df_x == 0) throw std::runtime_error("Derivative is zero. Newton's method fails.");
        x = x - f_x / df_x;
    }
    throw std::runtime_error("Maximum number of iterations reached. Newton's method did not converge.");
}

// secant method
inline double secant_method(std::function<double(double)> f, double x0, double x1, double total = 1e-8, int max_iter = 1000) {
    double f_x0 = f(x0);
    double f_x1 = f(x1);
    for (int i = 0; i < max_iter; ++i) {
        if (std::abs(f_x1) < total) return x1;
        if (f_x1 - f_x0 == 0) throw std::runtime_error("Division by zero in secant method.");
        double x2 = x1 - f_x1 * (x1 - x0) / (f_x1 - f_x0);
        x0 = x1;
        f_x0 = f_x1;
        x1 = x2;
        f_x1 = f(x1);
    }
    throw std::runtime_error("Maximum number of iterations reached. Secant method did not converge.");
}






} // namespace score::numerical_methods