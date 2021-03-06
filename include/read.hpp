#ifndef _READ_HPP_
#define _READ_HPP_

#include <iostream>
#include <string>

/* FOR split */
#include <sstream>
#include <vector>

/* FOR _uppercase_ */
#include <fstream>
#include <algorithm>
#include <cctype>
#include <functional>

/* FOR dmet */
#include "utils.hpp"
#include "lattice.hpp"
#include "mlfftrain.hpp"
#include "sgd.hpp"

void _read_inp_ (string&, LATTICE&, MLFFTRAIN&, SGD&);
void _read_data_ (string&, int, int, LATTICE&);
vvVectorXd _R_to_V_ (const vVectorXd&, double, const dv1&);

#endif
