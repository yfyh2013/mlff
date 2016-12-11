#include "include/mlfftrain.hpp"

#define UP(i,N) (i>=N)?(i-N):(i)
#define DOWN(i,N) (i<0)?(i+N):(i)
void _k_fold_partition_ (const VectorXd& v, VectorXd& vp, int k, int K)
{
    int N = v.rows (), Np = N / K * (K - 1), Nr = N - Np, istart = N / K * k;
    vp.setZero (Np);    //vr.setZero (N - Np);
    for (int i = 0; i < Np; i++)    vp(i) = v(UP(i+istart,N));
}

template <typename T>
void _k_fold_partition_ (const vector<T>& v, vector<T>& vp, vector<T>& vr,
    int k, int K)
{
    int N = v.size (), Np = N / K * (K - 1), Nr = N - Np, istart = N / K * k;
    for (int i = 0; i < Np; i++)    vp.push_back (v[UP(i+istart,N)]);
    for (int i = 0; i < Nr; i++)    vr.push_back (v[DOWN(istart-i-1,N)]);
}
#undef UP
#undef DOWN

double _mean_ (const vVectorXd& Ft)
{
    int N = Ft.size (), M = Ft[0].rows ();
    double mean = 0.;
    for (int i = 0; i < N; i++)
        mean += Ft[i].array ().abs ().sum () / (double) M;
    return mean / (double) N;
}

template <typename T>
void _insert_ (vector<T>& v1, const vector<T>& v2, const int Ns, const int Nt)
{
    int N1 = v1.size ();

    v1.insert (v1.end (), v2.begin () + Ns, v2.begin () + Ns + Nt - N1);
}

void MLFFTRAIN::_form_training_test_set_ (
    vvVectorXd& Vtrain, vVectorXd& Ftrain,
    vvVectorXd& Vtest, vVectorXd& Ftest,
    LATTICE& lat)
{
    //lat._form_sanity_set_ (Vtrain, Ftrain, Vtest, Ftest);
    _insert_ (Vtrain, lat.V, 0, Ntrain);
    _insert_ (Ftrain, lat.F, 0, Ntrain);
    lat._shuffle_fingerprint_ (Vtrain, Ftrain);
    _insert_ (Vtest, lat.V, Ntrain, Ntest);
    _insert_ (Ftest, lat.F, Ntrain, Ntest);
}

void MLFFTRAIN::_train_ (LATTICE& lat)
{
    vvVectorXd Vtrain_all, Vtest;
    vVectorXd Ftrain_all, Ftest;
    _form_training_test_set_ (Vtrain_all, Ftrain_all, Vtest, Ftest, lat);

    double Ftest_ave = _mean_ (Ftest);
    cout << "Ftest_ave = " << Ftest_ave << endl;
    krr.force_limit = Ftest_ave * 10;

    printf ("lambda\t\tvalid MAE\ttest MAE\ttest MARE\n");
    for (auto i = lbd_set.begin (); i < lbd_set.end (); i++)
    {
        double lbd = *i;
        double valid_MAE = 0.;
//  k-fold cross-validation
        for (int k = 0; k < K; k++)
        {
            krr._clear_all_ ();
            _k_fold_partition_ (Vtrain_all, krr.Vtrain, krr.Vvalid, k, K);
            _k_fold_partition_ (Ftrain_all, krr.Ftrain, krr.Fvalid, k, K);

            krr._init_ (lbd, gamma);
            krr._solve_ ("HQ");
            valid_MAE += krr._MAE_ (krr.Vvalid, krr.Fvalid);
        }
//  overall train
        krr._clear_all_ ();
        krr.Vtrain = Vtrain_all;    krr.Ftrain = Ftrain_all;
        krr._init_ (lbd, gamma);
        krr._solve_ ("HQ");
        krr._cmp_forces_ (Vtest, Ftest);

        double test_MAE = krr._MAE_ (Vtest, Ftest);
        printf ("%5.3e\t%9.6f\t%9.6f\t%9.6f\n",
            lbd, valid_MAE / K, test_MAE, test_MAE / Ftest_ave);
    }
}

void _form_testset_ (vvVectorXd& Vtest, vVectorXd& Ftest,
    LATTICE& lat, int Ntest, const vector<bool>& online)
{
    Vtest.clear (); Ftest.clear ();

    vvVectorXd Vpool;   vVectorXd Fpool;
    for (int i = 0; i < lat.V.size (); i++)
        if (online[i])
        {
            Vpool.push_back (lat.V[i]); Fpool.push_back (lat.F[i]);
        }
    lat._shuffle_fingerprint_ (Vpool, Fpool);
    if (Ntest > Vpool.size ())  Ntest = Vpool.size ();
    Vtest.assign (Vpool.begin (), Vpool.begin () + Ntest);
    Ftest.assign (Fpool.begin (), Fpool.begin () + Ntest);
}

void MLFFTRAIN::_1by1_train_ (LATTICE& lat)
{
    vvVectorXd Vtrain_all, Vtest;
    vVectorXd Ftrain_all, Ftest;
    //_form_training_test_set_ (Vtrain_all, Ftrain_all, Vtest, Ftest, lat);

    //printf ("lambda\t\tvalid MAE\ttest MAE\ttest MARE\n");
    _fancy_print_ ("generating basis", 3);
    for (auto i = lbd_set.begin (); i < lbd_set.end (); i++)
    {
        double lbd = *i;
        double valid_MAE = 0.;

        Vtrain_all.clear ();    Vtest.clear ();
        Ftrain_all.clear ();    Ftest.clear ();

        int Ninit = 100;
        vector<bool> online;    online.assign (lat.V.size (), true);
        Vtrain_all.assign (lat.V.begin (), lat.V.begin () + Ninit);
        Ftrain_all.assign (lat.F.begin (), lat.F.begin () + Ninit);
        for_each (online.begin (), online.begin () + Ninit,
                 [](bool d) {d = false;});

        int pos = Ninit;
        bool drain = false;
        while (Vtrain_all.size () < Ntrain)
        {
            if (online[pos - 1])
            {
                krr._clear_all_ ();
                krr.Vtrain = Vtrain_all;    krr.Ftrain = Ftrain_all;
                krr._init_ (lbd, gamma);

                krr._solve_ ("HQ");
            }

            double MAE_pos = krr._MAE_ (lat.V[pos], lat.F[pos]);
            if (MAE_pos > Fc)
            {
                Vtrain_all.push_back (lat.V[pos]);
                Ftrain_all.push_back (lat.F[pos]);
                online[pos] = false;
            }
            printf ("Error = %9.6f\tprocess: %4d/%4d\n",
                MAE_pos, Vtrain_all.size (), pos);

            // process bar
            double proc1 = (double) pos / lat.V.size ();
            double proc2 = (double) Vtrain_all.size () / Ntrain;
            double proc = (proc1 > proc2) ? (proc1) : (proc2);

            _progress_bar_ (proc);

            pos ++;
            if (pos == lat.V.size ())
            {
                drain = true;
                break;
            }
        }
        if (drain)  Ntrain = Vtrain_all.size ();

        _write_VF_ (Vtrain_all, Ftrain_all);

        cout << endl << endl;
        _fancy_print_ ("basis set finished", 3);
        printf ("%d configs are selected from %d (%5.2f%%)\n", Ntrain,
            (drain) ? (pos-1) : (pos), (double) Ntrain / lat.V.size () * 100);

        _form_testset_ (Vtest, Ftest, lat, Ntest, online);
        cout << "test set size: " << Vtest.size () << endl;

        double Ftest_ave = _mean_ (Ftest);
        cout << "Ftest_ave = " << Ftest_ave << endl;

//  k-fold cross-validation
        for (int k = 0; k < K; k++)
        {
            krr._clear_all_ ();
            _k_fold_partition_ (Vtrain_all, krr.Vtrain, krr.Vvalid, k, K);
            _k_fold_partition_ (Ftrain_all, krr.Ftrain, krr.Fvalid, k, K);

            krr._init_ (lbd, gamma);
            krr._solve_ ("HQ");
            valid_MAE += krr._MAE_ (krr.Vvalid, krr.Fvalid);
        }
//  overall train
        krr._clear_all_ ();
        krr.Vtrain = Vtrain_all;    krr.Ftrain = Ftrain_all;
        krr._init_ (lbd, gamma);
        krr._solve_ ("HQ");
        krr._cmp_forces_ (Vtest, Ftest);

        double test_MAE = krr._MAE_ (Vtest, Ftest);
        printf ("%5.3e\t%9.6f\t%9.6f\t%9.6f\n",
            lbd, valid_MAE / K, test_MAE, test_MAE / Ftest_ave);
    }
}

void MLFFTRAIN::_app_ (const LATTICE& lat)
{
    vVectorXd Fapp = krr._comput_forces_ (lat.Vapp);

    for (int i = 0; i < Fapp.size (); i++)
        printf ("%9.6f;%9.6f;%9.6f;%9.6f;%9.6f;%9.6f\n",
            lat.Rapp[0][i](0), lat.Rapp[0][i](1), lat.Rapp[0][i](2),
            Fapp[i](0), Fapp[i](1), Fapp[i](2));

    string fname (lat.out_path_app + "/data_pred.dat");
    FILE *pF = fopen (fname.c_str (), "w+");

    for (int i = 0; i < Fapp.size (); i++)
        fprintf (pF, "%9.6f;%9.6f;%9.6f;%9.6f;%9.6f;%9.6f\n",
            lat.Rapp[0][i](0), lat.Rapp[0][i](1), lat.Rapp[0][i](2),
            Fapp[i](0), Fapp[i](1), Fapp[i](2));
    fclose (pF);

    string fnamex (lat.out_path_app + "/fingerprint_x_pred.dat");
    string fnamey (lat.out_path_app + "/fingerprint_y_pred.dat");
    string fnamez (lat.out_path_app + "/fingerprint_z_pred.dat");
    FILE *pVx = fopen (fnamex.c_str (), "w+");
    FILE *pVy = fopen (fnamey.c_str (), "w+");
    FILE *pVz = fopen (fnamez.c_str (), "w+");

    for (int i = 0; i < lat.Vapp.size (); i++)
    {
        for (int j = 0; j < lat.Vapp[0][0].rows (); j++)
        {
            fprintf (pVx, "%9.6f;", lat.Vapp[i][0](j));
            fprintf (pVy, "%9.6f;", lat.Vapp[i][1](j));
            fprintf (pVz, "%9.6f;", lat.Vapp[i][2](j));
        }
        fprintf (pVx, "\n");    fprintf (pVy, "\n");    fprintf (pVz, "\n");
    }
    fclose (pVx);   fclose (pVy);   fclose (pVz);
}

void MLFFTRAIN::_write_VF_ (const vvVectorXd& V, const vVectorXd& F)
{
    if (V.size () != F.size ())
    {
        cout << "[Error] In MLFFTRAIN: V and F must have the same size." << endl;
        exit (1);
    }

    FILE *px = fopen ("output/training_VFx.dat", "w+");
    FILE *py = fopen ("output/training_VFy.dat", "w+");
    FILE *pz = fopen ("output/training_VFz.dat", "w+");
    for (int i = 0; i < V.size (); i++)
    {
        for (int j = 0; j < V[i][0].rows (); j++)
        {
            fprintf (px, "%9.6f;", V[i][0][j]);
            fprintf (py, "%9.6f;", V[i][1][j]);
            fprintf (pz, "%9.6f;", V[i][2][j]);
        }
        fprintf (px, "%9.6f\n", F[i][0]);
        fprintf (py, "%9.6f\n", F[i][1]);
        fprintf (pz, "%9.6f\n", F[i][2]);
    }
    fclose (px);    fclose (py);    fclose (pz);
}
