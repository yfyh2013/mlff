#include "include/utils.hpp"
#include "include/lattice.hpp"
#include "include/mlfftrain.hpp"
#include "include/krr.hpp"
#include "include/read.hpp"

int main (int argc, char *argv[])
{
    if (argc < 2)
    {
        cout << "Usage: inp" << endl;
        exit (1);
    }

    string fname_inp (argv[1]);
    LATTICE lat;
    MLFFTRAIN fft;
    _read_inp_ (fname_inp, lat, fft);
    //lat._gen_rdf_ (100);
    //lat._print_ ();
    lat._fingerprint_ ();
    fft._train_ (lat);

    return 0;
}
