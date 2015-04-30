// stitch2D.cpp : �ܼ� ���� ���α׷��� ���� �������� �����մϴ�.

#include <iostream>
#include <math.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>  // defines FILENAME_MAX 
#include <direct.h> //_getcwd


using namespace std;

#define IMAGE_YD		(int) 3008			//Ÿ�� �� �ҽ����� ����
#define IMAGE_XD		(int) 3072			//Ÿ�� �� �ҽ����� ��
#define IMAGE_PITCH		(double) 0.148		//Ÿ�� �� �ҽ����� �ȼ� ��ġ (mm)
#define IMAGE_BITS		(int) 16			//Ÿ�� �� �ҽ����� ��Ʈ��
#define GetCurrentDir _getcwd

#ifdef __cplusplus
extern "C"
{
#include <vips/vips.h>
#include <vips/util.h>
#include <vips/colour.h>
#include <vips/region.h>
#include <vips/rect.h>

#include <libxml/parser.h>

#define DIRSLASHWIN '\\'
#define DIRSLASHUNIX '/'

static char *
remove_ext_tiff( char *name )
{
    char *out = _strdup( name );
    char *p;

    /* Chop off '.tiff'.
     */
    if( !( p = strrchr( out, '.' ) ) )
    {
        im_error( "stitch2d", "Bad file name format '%s'", name );
        free( out );
        return( NULL );
    }
    *p = '\0';

    return( out );
}

static void * malloc_or_die( size_t size )
{
    void * p;
    p = malloc( size );
    if( p == NULL )
    {
#ifdef PRINT_MALLOC_ERRS
        fprintf( stderr, "Couldn't get memory (%u bytes)\n", ( unsigned int )size );
#endif
        exit( 1 );
        return NULL; /* Won't actually return... */
    }
    return p;
}

/* Find the root name of a file name. Return new, shorter, string.
 */
static char *
find_directory( char *file )
{
    char *dirname;
    char *lastslash;
    int   len;

    lastslash = strrchr( file, DIRSLASHWIN );
    len =  ( lastslash == NULL ) ? 0 : ( int ) ( lastslash - file );
    dirname = ( char * ) malloc_or_die ( sizeof( char ) * ( len + 2 ) );

    if ( len > 0 )
        strncpy( dirname, file, len );
    else if ( *file != DIRSLASHWIN )
    {
        *dirname = '.';
        len = 1;
    }
    else
    {
        *dirname = DIRSLASHWIN;
        len = 1;
    }

    dirname[len] = '\0';
    return dirname;
}

/* Find the root name of a file name. Return new, shorter, string.
 */
static char *
find_root( char *name )
{
    char *out = _strdup( name );
    char *p;

    /* Chop off '.v'.
     */
    if( !( p = strrchr( out, '.' ) ) )
    {
        im_error( "stitch2d", "Bad file name format '%s'", name );
        free( out );
        return( NULL );
    }
    *p = '\0';

    /* Chop off nxn.
     */
    if( !( p = strrchr( out, '.' ) ) )
    {
        im_error( "stitch2d", "Bad file name format '%s'", name );
        free( out );
        return( NULL );
    }
    *p = '\0';

    return( out );
}

/* Find the x position of a file name (extract n from nxm).
 */
static int
find_x( char *name )
{
    int n;
    char *p;
    char *out;
    out = _strdup( name );

    /* Find '.nxm'.
        */
    if( !( p = strrchr( out, '.' ) ) )
    {
        im_error( "stitch2d", "Bad argument '%s'", name );
        free( out );
        return( -1 );
    }
    /* Read out x posn.
     */
    if( sscanf_s( p, ".%dx%*d", &n ) != 1 )
    {
        im_error( "stitch2d", "Bad argument '%s'", name );
        free( out );
        return( -1 );
    }

    free( out );
    return( n );
}

/* Find the y position of a file name (extract m from .nxm).
 */
static int
find_y( char *name )
{
    int m;
    char *p;
    char *out = _strdup( name );

    if( !( p = strrchr( out, '.' ) ) )
    {
        im_error( "stitch2d", "Bad argument '%s'", name );
        free( out );
        return( -1 );
    }
    /* Read out y posn.
     */
    if( sscanf_s( p, ".%*dx%d", &m ) != 1 )
    {
        im_error( "stitch2d", "Bad file name format '%s'", name );
        free( out );
        return( -1 );
    }

    free( out );
    return( m );
}

}
#endif /*__cplusplus*/

class image2D
{
public:
    image2D( int n, int m, int sc = 1, int bits = 12 );
    image2D( unsigned short *data, int n, int m, int sc = 1, int bits = 12 );
    ~image2D();
    void		transformation2D( image2D& match, int trs_y, int trs_x );
    double	mutualinformation( image2D& match, int bins = 128 );
    void		mi_gradient_matching( image2D& crop, int& trs_y, int& trs_x, int max_trs, int iter_bound = 30, int bins = 128 );
    void		image_shrink( int sc = 1 );
    void		lower_bounding_image_values( int lb = 1 );
    int			get_xd()
    {
        return ( xd );
    }
    int			get_yd()
    {
        return ( yd );
    }
    int 		get_scale()
    {
        return ( scale );
    }
    void		stiching_2images( image2D& source, int& trs_y, int& trs_x, double pixel_pitch, double overlap );
    void		saving_stiching_result( image2D& source, int trs_y, int trs_x, char *filnename, int mode = 0 );
private:
    unsigned short  **image; // 2D image
    int xd, yd;             //image size in pixels
    int scale;              //relative to original image
    int bits_count;       //16, 12, 8 ...
};

//���� ���: ������ pixel ũ�⸦ sc ��ŭ Ű�� (sc�� 1, 2, 4, 8, .. �� 2�� power)
//����� �ƴ� �ܼ� sampling ��� ���
//xd, yd, scale�� ���� ����
void image2D::image_shrink( int sc )
{
    unsigned short **image_shrink;

    int t_yd, t_xd;

    t_yd = yd / sc;
    t_xd = xd / sc;

    //��� ���� ���� ���� �Ҵ�
    image_shrink = new unsigned short *[t_yd];
    for ( int y = 0; y < t_yd; y++ )
        image_shrink[y] = new unsigned short[t_xd];

    //sampling
    for ( int y = 0; y < t_yd; y++ )
        for ( int x = 0; x < t_xd; x++ )
            image_shrink[y][x] = image[y * sc][x * sc];

    //�޸� ����: image
    for ( int y = 0; y < yd; y++ ) delete image[y];

    //������ ���Ҵ�
    yd = t_yd;
    xd = t_xd;
    scale *= sc;

    //��ҵ� ������ image�� ���Ҵ�
    image = image_shrink;

}



// image2D ������: �� ���� image[ ][ ] ����
image2D::image2D( int n, int m, int sc, int bits ):
    yd( n ), xd( m ), scale( sc ), bits_count( bits )
{

    if ( ( n < 1 ) || ( m < 1 ) )
    {
        cerr << "illegal image_target size: " << m << ", " << n;
        exit( 1 );
    }
    image = new unsigned short *[yd];
    for ( int y = 0; y < yd; y++ )
        image[y] = new unsigned short[xd];

}

// image2D ������: 1���� �迭���� image[ ][ ] ����
image2D::image2D( unsigned short *data, int n, int m, int sc, int bits ):
    yd( n ), xd( m ), scale( sc ), bits_count( bits )
{

    if ( ( n < 1 ) || ( m < 1 ) )
    {
        cerr << "illegal image_target size: " << m << ", " << n;
        exit( 1 );
    }

    image = new unsigned short *[yd];
    for ( int y = 0; y < yd; y++ )
        image[y] = new unsigned short[xd];

    for ( int y = 0; y < yd; y++ )
        for ( int x = 0; x < xd; x++ )
            image[y][x] = data[y * xd + x];

}

//���󿡼� 0�� ������ 1�� ������ �ٲ�: Ÿ�ٿ��󿡸� �����ϸ�, mutual informatioon ���� Ÿ�ٿ��� �ۿ� �ִ� �ȼ��� ������ 0���� �ϰ� ��꿡�� �����ϱ� ����
void image2D::lower_bounding_image_values( int lb )
{

    for ( int y = 0; y < yd; y++ )
        for ( int x = 0; x < xd; x++ )
            if ( image[y][x] < lb ) image[y][x] = lb;

}


//��Ī�� 2���� ���� �̵� (trs_y, trs_x)
void image2D::transformation2D( image2D& match, int trs_y, int trs_x )
{

    unsigned short   val;
    int    x_m, y_m;
    int    sc;
    int    t_xd, t_yd;

    t_xd = this->xd;  //xd�� �ᵵ ������, ���ظ� ���� �ϱ�
    t_yd = this->yd;  //
    sc = match.scale;

    //match ���� �����̵�(trs_x, trs_y)
    for ( int y = 0; y < match.yd; y++ )
        for ( int x = 0; x < match.xd; x++ )
        {
            x_m = ( x * sc + trs_x );
            y_m = ( y * sc + trs_y );

            //Ÿ�ٿ���(this)���� �ȼ��� ��������
            // �ȼ����� 0�� ���� ��� �ܺ� �ȼ��� ��쿡 �ش�: Ÿ�� ������ lower bound�� 1�� �Ǿ� �ֱ⿡..
            if ( ( y_m < t_yd ) &&
                    ( x_m < t_xd ) &&
                    ( x_m >= 0 )   &&
                    ( y_m >= 0 ) )
                val = this->image[y_m][x_m];
            else
                val = 0;

            match.image[y][x] = val;
        }

}

//2���� ���� mutual information ���
double image2D::mutualinformation( image2D& match, int bins )
{
    int image_bits;
    unsigned short val_s, val_m;
    double **j_hist;
    double *prob_x, *prob_y;
    double j_sum, mi;


    if ( ( this->yd != match.yd ) || ( this->xd != match.xd ) )
    {
        cout << "Difference images sizes between Source & Match" << endl;
        exit( 1 );
    }

    j_hist = new double *[bins];
    for ( int i = 0; i < bins; i++ ) j_hist[i] = new double[bins];
    prob_x = new double [bins];
    prob_y = new double [bins];

    for ( int m = 0; m < bins; m++ )
        for ( int n = 0; n < bins; n++ )
            j_hist[m][n] = 0.0;

    for ( int m = 0; m < yd; m++ )
        for ( int n = 0; n < xd; n++ )
        {
            val_s = this->image[m][n];
            val_m = match.image[m][n];
            if ( val_m > 0 )
            {

                if ( this->bits_count > match.bits_count )
                    image_bits = this->bits_count;
                else
                    image_bits = match.bits_count;

                // convert image_bits to log_2(bins) bits
                if ( bins < ( 2 << ( image_bits - 1 ) ) )
                {
                    val_s = val_s / ( unsigned short )( ( int )( 2 << ( image_bits - 1 ) ) / bins );
                    val_m = val_m / ( unsigned short )( ( int )( 2 << ( image_bits - 1 ) ) / bins );
                    j_hist[val_s][val_m] += 1.0;
                }
                else
                {
                    j_hist[val_s][val_m] += 1.0;
                }
            }
        }

    j_sum = 0.0;
    for ( int l = 0; l < bins; l++ )
        for ( int m = 0; m < bins; m++ )
            j_sum += j_hist[l][m];

    if ( j_sum > 0.0 )
    {
        for ( int l = 0; l < bins; l++ )
            for ( int m = 0; m < bins; m++ )
                j_hist[l][m] /= j_sum;
    }


    for ( int l = 0; l < bins; l++ )
    {
        prob_x[l] = 0;
        for ( int m = 0; m < bins; m++ )
            prob_x[l] += j_hist[l][m];

    }
    for ( int m = 0; m < bins; m++ )
    {
        prob_y[m] = 0;
        for ( int l = 0; l < bins; l++ )
            prob_y[m] += j_hist[l][m];

    }

    mi = 0.0;

    //mutual information ���
    for ( int l = 0; l < bins; l++ )
        for ( int m = 0; m < bins; m++ )
        {
            if ( prob_x[l] * prob_y[m] > 1.0e-10 )
                if ( j_hist[l][m] > 1.0e-10 )
                    mi += j_hist[l][m] * log( j_hist[l][m] / ( prob_x[l] * prob_y[m] ) );
        }

    for ( int i = 0; i < bins; i++ )
        delete j_hist[i];
    delete j_hist;
    delete prob_x;
    delete prob_y;

    return mi;
}



//Mutual information�� �̿��� ���� ��Ī
//1. crop����� ������ ũ���� match ���� ����
//2. match ������ �·Ḧ 2D ��ȯ�ϸ鼭 Ÿ�ٿ���(this->image)���� �ȼ��� ��������
//3. crop vs match ������ mutual information ���
//4. mi�� gradient search ��� ����Ͽ� ���� 2D ��ȯ�� search
void image2D::mi_gradient_matching( image2D& crop, int& trs_y, int& trs_x, int max_trs, int iter_bound, int bins )
{

    int MAX_TRS = 1;
    int MIN_TRS = 1;

    //MAX_TRS ����
    // 1, 2, 4, 8, 16, 32, 64, ... �߿��� max_trs ������ �ִ밪 ����
    do
    {
        if ( max_trs > MAX_TRS )
            MAX_TRS *= 2;
    }
    while ( ( 2 * MAX_TRS ) <= max_trs );

    //�߰��� 4-nearest neighbor�� ��ǥ�� ����
    int trs_step_x[5] = {0, MAX_TRS,       0, -MAX_TRS,        0};
    int trs_step_y[5] = {0,       0, MAX_TRS,        0, -MAX_TRS};

    double mi[5], mi_max, pre_mi_max;
    int    mi_max_index, iter = 0;

    //match ����� ��ü ����: crop�� ũ�� �� ���� ����
    image2D match( crop.yd, crop.xd, crop.scale, crop.bits_count );

    pre_mi_max = mi_max = 0.0;

    do
    {
        pre_mi_max = mi_max;
        mi_max_index = 0;
        for ( int i = 0; i < 5; i++ )
        {
            //Ÿ��(this)���� 2D ��ȯ�� match ���� ����
            this->transformation2D( match, trs_y + trs_step_y[i], trs_x + trs_step_x[i] );
            //crop�� match�� mi ���
            mi[i] = crop.mutualinformation( match, bins );
            //mi �ִ���� search
            if ( mi[i] > mi_max )
            {
                mi_max_index = i;
                mi_max = mi[i];
            }
        }

        //mi �ִ� ���� �������� 2D ���� �̵�
        trs_y += trs_step_y[mi_max_index];
        trs_x += trs_step_x[mi_max_index];

        cout << iter << "-th iteration: mi = " << mi_max << "; dx=" << trs_step_x[1] << "; " << trs_x << " " << trs_y  << endl;

        iter++;

        //mi ��ȭ�� ������, step ��ȭ���� 1/2�� ����
        if ( !( mi_max > pre_mi_max ) )
        {
            for ( int i = 0; i < 5; i++ )
            {
                trs_step_x[i] /= 2;
                trs_step_y[i] /= 2;
            }
        }
    }
    while ( ( trs_step_x[1] >= MIN_TRS ) && ( iter < iter_bound ) );

}


image2D::~image2D()
{
    for ( int y = 0; y < yd; y++ ) delete image[y];
    delete image;

}

//�� ������ stitching�Ͽ� raw �����͸� ���Ͽ� ����
//mode = 0: �ڿ������� stitching, 1: Ÿ�� ����, 2: �ҽ� ����
void	image2D::saving_stiching_result( image2D& source, int trans_y, int trans_x, char *filename, int mode )
{
    if ( ( trans_y < this->yd ) && ( trans_y >= 0 ) && ( trans_x > ( -this->xd / 5 ) ) && ( trans_x < ( this->xd / 5 ) ) )
    {


        int sum_yd, sum_xd;
        unsigned short temp = 0, v_t, v_s, val;
        double  r_t, r_s;

        sum_yd = this->yd + trans_y;  		  //stitching���� ũ��
        sum_xd = this->xd + abs( trans_x );	 //stitching���� ũ��

        cout << "��ģ ���� : (" << sum_xd << ", " << sum_yd << ")" << endl;

        FILE *fp;
        errno_t error_fd;
        error_fd = fopen_s( &fp, filename, "wb" );

        //Ÿ�� ������ �ҽ� ����� ��ġ�� �ʴ� �κ� ����
        for ( int y = 0; y < trans_y; y++ )
        {
            if ( trans_x >= 0 )
            {
                for ( int x = 0; x < this->xd; x++ )
                    fwrite( &this->image[y][x], sizeof( unsigned short ), 1, fp );
                for ( int x = 0; x < trans_x; x++ )
                    fwrite( &temp, sizeof( unsigned short ), 1, fp );
            }
            else
            {
                for ( int x = trans_x; x < 0; x++ )
                    fwrite( &temp, sizeof( unsigned short ), 1, fp );
                for ( int x = 0; x < this->xd; x++ )
                    fwrite( &this->image[y][x], sizeof( unsigned short ), 1, fp );
            }
        }
        //Ÿ�� ����� �ҽ� ������ ��ġ�� �κ� �ڿ������� weight�� �༭ ����
        for ( int y = trans_y; y < this->yd; y++ )
        {
            r_t = ( y - trans_y ) / ( double )( this->yd - trans_y ); //never 0 on denominator (trans_y < this->yd)
            r_s = ( this->yd - y ) / ( double )( this->yd - trans_y ); //never 0

            if ( mode == 1 )
            {
                r_t = 0.0;    // Ÿ�� ����
                r_s = 1.0;
            }
            else if ( mode == 2 )
            {
                r_t = 1.0;    //�ҽ� ����
                r_s = 0.0;
            }

            if ( trans_x >= 0 )
            {
                for ( int x = 0; x < this->xd; x++ )
                {
                    v_t = this->image[y][x];
                    if ( x >= trans_x ) v_s = source.image[y - trans_y][x - trans_x];
                    else v_s = 0;
                    val = ( unsigned short )( v_t * r_s + v_s * r_t );
                    fwrite( &val, sizeof( unsigned short ), 1, fp );
                }
                for ( int x = this->xd; x < this->xd + trans_x; x++ )
                {
                    v_t = 0;
                    v_s = source.image[y - trans_y][x - trans_x];
                    val = ( unsigned short )( v_t * r_s + v_s * r_t );
                    fwrite( &val, sizeof( unsigned short ), 1, fp );
                }
            }
            else
            {
                for ( int x = trans_x; x < 0; x++ )
                {
                    v_t = 0;
                    v_s = source.image[y - trans_y][x - trans_x];
                    val = ( unsigned short )( v_t * r_s + v_s * r_t );
                    fwrite( &val, sizeof( unsigned short ), 1, fp );
                }
                for ( int x = 0; x < this->xd; x++ )
                {
                    v_t = this->image[y][x];
                    if ( x < ( this->xd + trans_x ) ) v_s = source.image[y - trans_y][x - trans_x];
                    else v_s = 0;
                    val = ( unsigned short )( v_t * r_s + v_s * r_t );
                    fwrite( &val, sizeof( unsigned short ), 1, fp );
                }
            }
        }
        //�ҽ������� Ÿ�ٿ���� ��ġ�� �ʴ� �κ� ����
        for ( int y = ( this->yd - trans_y ); y < this->yd; y++ )
        {
            if ( trans_x >= 0 )
            {
                for ( int x = 0; x < trans_x; x++ )
                    fwrite( &temp, sizeof( unsigned short ), 1, fp );
                for ( int x = 0; x < this->xd; x++ )
                    fwrite( &source.image[y][x], sizeof( unsigned short ), 1, fp );
            }
            else
            {
                for ( int x = 0; x < this->xd; x++ )
                    fwrite( &source.image[y][x], sizeof( unsigned short ), 1, fp );
                for ( int x = trans_x; x < 0; x++ )
                    fwrite( &temp, sizeof( unsigned short ), 1, fp );

            }
        }
        fclose( fp );
    }
}

//Ÿ�� ����� �ҽ������� ��Ī�ϴ� �� �˰���
//�ҽ������� �ȼ���ġ�� overlap ũ�⸦ ����Ͽ� ��Ī�� crop ���� ����
//crop ���� ������ overlap�� 60mm���� ũ�� 60mm�� �ش��ϴ� ���������� ����
//crop ������ ����ؼ� ���δ��� ����
//�������� Ÿ�� (0,0) ��ġ ��� crop ������ ��Ī��ġ(trans_y, trans_x) ������
void image2D::stiching_2images( image2D& source, int& trans_y, int& trans_x, double pixel_pitch, double overlap )
{
    int crop_yd;

    //Ÿ�� ���󿡼� �ȼ����� 0�� ��� +1 shift: �ȼ��� 0�� ���� ���ۿ� �Ҵ��ϰ� mutual information ��꿡�� ����
    this->lower_bounding_image_values( 1 );

    //�ҽ� ���󿡼� ��Ī�� ����(crop) ��������: ���� crop_yd
    if ( overlap < 60.0 )
    {
        crop_yd = ( int )( overlap / pixel_pitch ); // �ִ� 60mm ���̱����� ��Ī�� ���
    }
    else
    {
        crop_yd = ( int )( 60.0 / pixel_pitch );
    }

    if ( crop_yd > source.yd )
    {
        crop_yd = source.yd; // Ȥ��, crop ���̰� �ҽ� ���̺��� Ŀ���� ��� ����
    }

    image2D crop( crop_yd, source.xd, source.scale, source.bits_count );
    cout << "Crop ���� : (" << crop.get_xd() << ", " << crop.get_yd() << "), scale = " << crop.get_scale() <<  endl;

    //crop���� ��������
    for ( int y = 0; y < crop_yd; y++ )
    {
        for ( int x = 0; x < source.xd; x++ )
        {
            crop.image[y][x] = source.image[y][x];
        }
    }

    //crop ���� ���: scale=1,2,4,8,16, ....
    crop.image_shrink( 4 ); //USER OPT *************************.
    cout << "Crop ��� : (" << crop.get_xd() << ", " << crop.get_yd() << "), scale = " << crop.get_scale() <<  endl;

    //crop ������ 2D �̵��ϴ� �ʱⰪ ����
    // Ÿ�� ���� �ϴܰ� �ҽ����� ���(crop) ��ø���� ��ħ(overlap)�� �밭 �ľ��Ͽ� Ÿ�� �ϴܿ� crop�� ����
    trans_x = this->get_xd() / 2 - ( crop.get_xd() / 2 * crop.get_scale() );
    trans_y = this->get_yd()   - ( int )( overlap / pixel_pitch );

    cout << "�ʱ� ��Ī ��ġ : (" << trans_x << ", " << trans_y << ")" << endl;

    int 	max_trs = ( int )( 8.0 / pixel_pitch ); //USER OPT******.  ��Īġ ��ġ �Ķ���� ������Ʈ step�� �ִ� 8mm�� �� (����ġ)
    int     iteration = 50;                       //USER OPT******. ��Ī �˰��� �ݺ���
    int     histogram_bins = 128;             //USER OPT******. mutual information ���� ������׷� bin ��

    //�� ��Ī �Լ�: crop������ Ÿ�ٿ��� ��Ī�ϰ� ��Ī�����(trans_y, trans_x) arguments�� ����
    this->mi_gradient_matching( crop, trans_y, trans_x, max_trs, iteration, histogram_bins );

    cout << "���� ��Ī ��ġ : (" << trans_x << ", " << trans_y << ")" << endl;
}

int main( int argc, char **argv )
{
    if ( argc < 2 || ( strcmp( argv[1], "mosaic" ) != 0 &&
                       strcmp( argv[1], "merge" ) != 0 ) )
    {
        return 1;
    }

    if ( strcmp( argv[1], "mosaic" ) == 0 )
    {
        char name[ 1024 ];
        char *file_root = NULL;
        int fd_list_point_file;
        errno_t err_no_pt_file;
        int number_points = 0;
        err_no_pt_file = _sopen_s( &fd_list_point_file, argv[2], _O_CREAT | _O_WRONLY | _O_TEXT | _O_TRUNC,
                                   _SH_DENYNO,
                                   _S_IREAD | _S_IWRITE );
        for ( int i = 3; i < argc - 1; i++ )
        {
            IMAGE * img_convert;
            int fd;
            errno_t err_no;
            char * output_file_v;
            char * output_file_raw;
            file_root = remove_ext_tiff( argv[i] );
            if( !file_root )
                error_exit( "error at file_root" );
            im_snprintf( name, 1024, "%s.v", file_root );
            output_file_v = _strdup( name );

            im_snprintf( name, 1024, "%s.raw", file_root );
            output_file_raw = _strdup( name );
            if( !( img_convert = im_open( output_file_v, "w" ) ) )
                error_exit( "find_mosaic: unable to open %s for input", output_file_v );

            im_tiff2vips( argv[ i ], img_convert );

            if ( _access( output_file_raw, 0 ) == -1 )
            {
                err_no = _sopen_s( &fd, output_file_raw, _O_CREAT | _O_WRONLY | _O_BINARY | _O_TRUNC, _SH_DENYNO, _S_IREAD | _S_IWRITE );
                if ( img_convert->Bands > 1 )
                {
                    IMAGE * img_extract_band = im_open( "extract_band", "p" );
                    im_extract_band( img_convert, img_extract_band, 0 );
                    im_vips2raw( img_extract_band, fd );
                    im_close( img_extract_band );
                }
                else
                {
                    im_vips2raw( img_convert, fd );
                }
                _close( fd );
            }

            FILE *fp;
            unsigned short *data;

            //Ÿ�� ���� �ε�: 1���� unsigned short �迭���� �о����
            //Ÿ�� ���� �ϴ�(�迭 �Ĺݺ�)�� �Ʒ��� �ҽ����� ���(�迭 ��ݺ�)�� ��Ī�Ǵ� �����̾�� ��
            data = new unsigned short[img_convert->Ysize * img_convert->Xsize];
            errno_t error_fd;
            error_fd = fopen_s( &fp, output_file_raw, "rb" ); // "E:\\coreline-prj\\Stitch\\Stitch\\wholebody\\3-Hip.raw"
            if ( fp != 0 )
            {
                for ( int i = 0; i < img_convert->Ysize * img_convert->Xsize; i++ )
                    fread( &data[i], sizeof( unsigned short ), 1, fp );
                cout << "Ÿ�� ���� �ε�: ";
            }
            else
            {
                cout << "Ÿ�� ���� ����!\n";
                exit( 1 );
            }

            fclose( fp );

            //data�� �̿��Ͽ� image2D ���� target ����
            image2D target( data, img_convert->Ysize, img_convert->Xsize, 1, IMAGE_BITS );
            cout << "(" << target.get_xd() << ", " << target.get_yd() << "), scale = " << target.get_scale() <<  endl;

            im_close( img_convert );

            file_root = remove_ext_tiff( argv[i + 1] );
            if( !file_root )
                error_exit( "error at file_root" );
            im_snprintf( name, 1024, "%s.v", file_root );
            output_file_v = _strdup( name );

            im_snprintf( name, 1024, "%s.raw", file_root );
            output_file_raw = _strdup( name );
            if( !( img_convert = im_open( output_file_v, "w" ) ) )
                error_exit( "find_mosaic: unable to open %s for input", output_file_v );

            im_tiff2vips( argv[i + 1], img_convert );
            if ( _access( output_file_raw, 0 ) == -1 )
            {
                err_no = _sopen_s( &fd, output_file_raw, _O_CREAT | _O_WRONLY | _O_BINARY | _O_TRUNC, _SH_DENYNO, _S_IREAD | _S_IWRITE );
                if ( img_convert->Bands > 1 )
                {
                    IMAGE * img_extract_band = im_open( "extract_band", "p" );
                    im_extract_band( img_convert, img_extract_band, 0 );
                    im_vips2raw( img_extract_band, fd );
                    im_close( img_extract_band );
                }
                else
                {
                    im_vips2raw( img_convert, fd );
                }
                _close( fd );
            }

            //�ҽ� ���� �ε�: 1���� unsigned short �迭���� �о����
            //Ÿ�� ���� �ϴ��� �ҽ����� ����� ��Ī�Ǵ� �����̾�� ��
            error_fd = fopen_s( &fp, output_file_raw, "rb" ); // "E:\\coreline-prj\\Stitch\\Stitch\\wholebody\\4-Knee.raw"
            if ( fp != 0 )
            {
                for ( int i = 0; i < img_convert->Ysize * img_convert->Xsize; i++ )
                    fread( &data[i], sizeof( unsigned short ), 1, fp );
                cout << "�ҽ� ���� �ε�: ";
            }
            else
            {
                cout << "�ҽ� ���� ����!\n";
                exit( 1 );
            }
            fclose( fp );
            //data�� �̿��Ͽ� image2D ���� source ����
            image2D source( data, img_convert->Ysize, img_convert->Xsize, 1, IMAGE_BITS );
            cout << "(" << source.get_xd() << ", " << source.get_yd() << "), scale = " << source.get_scale() <<  endl;

            delete data;
            im_close( img_convert );

            int trans_x, trans_y;  // ���� �̵� ��ǥ��
            double pixel_pitch = IMAGE_PITCH; //������ �ȼ� ��ġ (mm)
            double overlap = 60.0; //������ �Կ��� ���� �������� ������ �� (mm): �������� 10~20mm �̻� ���̳��� ��Ī�� ����� // 60mm ��õ
            int mode = 1;        // stitching ��� ���� ���: 0 ������ �ε巴��, 1 Ÿ�� ����, 2 �ҽ� ����
            target.stiching_2images( source, trans_y, trans_x, pixel_pitch, overlap );

            char points[ 28 ];
            char * output_point;
            memset( points, 0, 28 );
            im_snprintf( points, 28, "%d , %d\r\n", trans_x, trans_y );
            output_point = _strdup( points );
            _write( fd_list_point_file, output_point, strlen( output_point ) );
            number_points = i - 3;

            free( output_file_v );
            free( output_file_raw );
            //target.saving_stiching_result( source, trans_y, trans_x, "sum.raw", mode );
        }

        char total_points[ 128 ];
        char * output_total_points;
        memset( total_points, 0, 128 );
        im_snprintf( total_points, 128, "Number of Point: %d", number_points + 1 );
        output_total_points = _strdup( total_points );
        _write( fd_list_point_file, output_total_points, strlen( output_total_points ) );
        _close( fd_list_point_file );
        free( file_root );
        return 0;
    }

    if ( strcmp( argv[1], "merge" ) == 0 )
    {
		if (_access("stitchconfig.xml", 0) != -1)
		{
			char cCurrentPath[FILENAME_MAX];
			LIBXML_TEST_VERSION
			if (!GetCurrentDir(cCurrentPath, sizeof(cCurrentPath)))
			{
				return errno;
			}
			cCurrentPath[sizeof(cCurrentPath) - 1] = '\0'; 
			xmlDoc *doc;
			xmlNode *node;

			// Cleanup function for the XML library.
			xmlCleanupParser();
		}

        char *file_root = NULL;
        int number_of_point = atoi( argv[2] );
        int k = 0;
        IMAGE * tmpOut[6];
        IMAGE * tmpOut1[6];
        IMAGE * sourceimg;
        IMAGE * targetimg;

#if 0
        if( !( tmpOut = im_open( "tmpOut.v", "p" ) ) ||
                !( tmpOut1 = im_open( "tmpOut1.v", "p" ) ) )
            error_exit( "stitch2d: unable to open file for output" );
#endif
        for ( int i = argc - 1; i > argc - ( number_of_point + 1 ); i-- )
        {
            char name[1024];
            char * output_file_v;
            int trans_x, trans_y;
            file_root = remove_ext_tiff( argv[ i - 1 ] );
            if( !file_root )
                error_exit( "error at file_root" );
            im_snprintf( name, 1024, "%s.v", file_root );
            output_file_v = _strdup( name );
            if ( k <= number_of_point )
            {
                trans_x = find_x( argv[ 2 + number_of_point - k ] );
                trans_y = find_y( argv[ 2 + number_of_point - k ] );
            }

            if ( _access( output_file_v, 0 ) == -1 )
            {
                IMAGE * save_Out;
                if( !( save_Out = im_open( output_file_v , "w" ) ) )
                    error_exit( "stitch2d: unable to open file for output" );
                im_tiff2vips( argv[ i - 1 ], save_Out ); //, IMAGE_XD, IMAGE_YD, 1, 0 );
                im_close( save_Out );
            }

            if( !( sourceimg = im_open( output_file_v , "r" ) ) )
                error_exit( "stitch2d: unable to open file for output" );

            if ( k == 0 )
            {
                im_open_local_array( sourceimg, tmpOut, 6, "tmpOut.v", "p" ) ;
                im_open_local_array( sourceimg, tmpOut1, 6, "tmpOut1.v", "p" );
            }

            ////////// SRC IMAGE ////////////

            if ( k > 0 && tmpOut1[k - 1]->Ysize > sourceimg->Ysize ) //( _access( output_file_i_v, 0 ) != -1)
            {
#if 0
#endif
            }
            else
            {

                file_root = remove_ext_tiff( argv[ i ] );
                if( !file_root )
                    error_exit( "error at file_root" );
                im_snprintf( name, 1024, "%s.v", file_root );
                output_file_v = _strdup( name );

                if ( _access( output_file_v, 0 ) == -1 )
                {
                    IMAGE * save_Out;
                    if( !( save_Out = im_open( output_file_v , "w" ) ) )
                        error_exit( "stitch2d: unable to open file for output" );
                    im_tiff2vips( argv[ i ], save_Out ); //, IMAGE_XD, IMAGE_YD, 1, 0 );
                    im_close( save_Out );
                }
                if( !( targetimg = im_open( output_file_v, "r" ) ) )
                    error_exit( "stitch2d: unable to open file for output" );
            }

            if ( k > 0 && tmpOut1[k - 1]->Ysize > sourceimg->Ysize )
            {
// 				im_tbmosaic1( sourceimg, tmpOut1[k - 1], tmpOut[k], 0, trans_x, trans_y, 0, 0, 
// 					trans_x + 400, trans_y + 400, 400, 400, 
// 					5, 30, 0, 100 );
                im_tbmosaic( sourceimg, tmpOut1[k - 1], tmpOut[k], 0, trans_x, trans_y, 0, 0, 5, 15, 0, 100 );
                //im_tbmerge(sourceimg, tmpOut1[k - 1], tmpOut[k], 0 - trans_x, 0 - trans_y, 100 );
            }
            else
            {
// 				im_tbmosaic1( sourceimg, targetimg, tmpOut[k], 0, trans_x, trans_y, 0, 0, 
// 					trans_x + 400, trans_y + 400, 0 + 400, 0 + 400, 
// 					5, 30, 0, 100 );
                im_tbmosaic( sourceimg, targetimg, tmpOut[k], 0, trans_x, trans_y, 0, 0, 5, 15, 0, 100 );
                //im_tbmerge(sourceimg, targetimg, tmpOut[k], 0 - trans_x, 0 - trans_y, 100 );
            }

#if 1
            im_global_balance( tmpOut[k], tmpOut1[k], 0.8 );
#endif

            k++;

            free( output_file_v );
        }

        if ( k > 0 )
        {
#if 1
            char name[1024];
            char * directory = find_directory( argv[argc - 1] );
            char * output_file_final_stitch_not_balance;
            char * output_file_final_stitch;

            im_snprintf( name, 1024, "%s\\finalstitch.tif", directory );
            output_file_final_stitch = _strdup( name );

            im_snprintf( name, 1024, "%s\\finalstitch_not_balance.tif", directory );
            output_file_final_stitch_not_balance = _strdup( name );

            im_vips2tiff( tmpOut[k - 1], output_file_final_stitch_not_balance );
            im_vips2tiff( tmpOut1[k - 1], output_file_final_stitch );

            free( directory );
            free( output_file_final_stitch );
            free( output_file_final_stitch_not_balance );
#endif
        }

        im_close( sourceimg );
        im_close( targetimg );
        free( file_root );
    }
    return 0;
}
