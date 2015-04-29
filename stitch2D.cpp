// stitch2D.cpp : 콘솔 응용 프로그램에 대한 진입점을 정의합니다.
// 2014.3.2 : haison

#include <iostream>
using namespace std;
#include <math.h>

#ifdef __cplusplus
extern "C" {
#include <vips/vips.h>
#include <vips/util.h>
#include <vips/colour.h>
#include <vips/region.h>
#include <vips/rect.h>

#define NUM_FILES 1000
#define MAXPOINTS 60
#define DIRSLASHWIN '\\'          
#define DIRSLASHUNIX '/'          

int xoverlap;
int yoverlap;

/*
extern int im_lrmerge();
extern int im_updatehist();
extern int im_merge_analysis();
extern int im__find_lroverlap();
extern int im__find_tboverlap();
*/

static int file_ptr = 0;
static IMAGE *in[ NUM_FILES ];

/* Strategy: build a tree describing the sequence of joins we want. Walk the
 * tree assigning temporary file names, compile the tree into a linear
 * sequence of join commands.
 */

/* Decoded file name info.
 */
static char *file_root = NULL;
static char *output_file = NULL;
static int width = 0;		/* Number of frames across */
static int height = 0;		/* Number of frames down */

static int file_list[ NUM_FILES ];

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

static void * malloc_dr_die(size_t size)
{
	void * p;
	p = malloc(size);
	if(p == NULL)
	{
#ifdef PRINT_MALLOC_ERRS
		fprintf(stderr, "Couldn't get memory (%u bytes)\n", (unsigned int)size);
#endif
		exit(1);
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

	lastslash = strrchr(file, DIRSLASHWIN);
	len =  (lastslash == NULL) ? 0 : (int) (lastslash - file);
	dirname = (char *) malloc_dr_die (sizeof(char) * (len+2));
	
	if (len > 0)
		strncpy(dirname, file, len);
	else if (*file != DIRSLASHWIN) 
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

//영상 축소: 영상의 pixel 크기를 sc 만큼 키움 (sc는 1, 2, 4, 8, .. 등 2의 power)
//평균이 아닌 단순 sampling 방법 사용
//xd, yd, scale도 따라서 변동
void image2D::image_shrink( int sc )
{
    unsigned short **image_shrink;

    int t_yd, t_xd;

    t_yd = yd / sc;
    t_xd = xd / sc;

    //축소 영상 저장 영역 할당
    image_shrink = new unsigned short *[t_yd];
    for ( int y = 0; y < t_yd; y++ )
        image_shrink[y] = new unsigned short[t_xd];

    //sampling
    for ( int y = 0; y < t_yd; y++ )
        for ( int x = 0; x < t_xd; x++ )
            image_shrink[y][x] = image[y * sc][x * sc];

    //메모리 해제: image
    for ( int y = 0; y < yd; y++ ) delete image[y];

    //변수값 재할당
    yd = t_yd;
    xd = t_xd;
    scale *= sc;

    //축소된 영상을 image에 재할당
    image = image_shrink;

}



// image2D 생성자: 빈 공간 image[ ][ ] 생성
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

// image2D 생성자: 1차원 배열에서 image[ ][ ] 생성
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

//영상에서 0인 레벨을 1로 모조리 바꿈: 타겟영상에만 적용하며, mutual informatioon 계산시 타겟영상 밖에 있는 픽셀의 레벨을 0으로 하고 계산에서 제외하기 위함
void image2D::lower_bounding_image_values( int lb )
{

    for ( int y = 0; y < yd; y++ )
        for ( int x = 0; x < xd; x++ )
            if ( image[y][x] < lb ) image[y][x] = lb;

}


//매칭용 2차원 영상 이동 (trs_y, trs_x)
void image2D::transformation2D( image2D& match, int trs_y, int trs_x )
{

    unsigned short   val;
    int    x_m, y_m;
    int    sc;
    int    t_xd, t_yd;

    t_xd = this->xd;  //xd로 써도 되지만, 이해를 쉽게 하기
    t_yd = this->yd;  //
    sc = match.scale;

    //match 영상 평행이동(trs_x, trs_y)
    for ( int y = 0; y < match.yd; y++ )
        for ( int x = 0; x < match.xd; x++ )
        {
            x_m = ( x * sc + trs_x );
            y_m = ( y * sc + trs_y );

            //타겟영상(this)에서 픽셀값 가져오기
            // 픽셀값이 0인 경우는 경계 외부 픽셀인 경우에 해당: 타겟 영상은 lower bound가 1로 되어 있기에..
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



//2개의 영상간 mutual information 계산
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

    //mutual information 계산
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



//Mutual information을 이용한 영상 매칭
//1. crop영상과 동일한 크기의 match 영상 생성
//2. match 영상의 좌료를 2D 변환하면서 타겟영상(this->image)에서 픽셀값 가져오기
//3. crop vs match 영상의 mutual information 계산
//4. mi의 gradient search 방법 사용하여 최적 2D 변환값 search
void image2D::mi_gradient_matching( image2D& crop, int& trs_y, int& trs_x, int max_trs, int iter_bound, int bins )
{

    int MAX_TRS = 1;
    int MIN_TRS = 1;

    //MAX_TRS 결정
    // 1, 2, 4, 8, 16, 32, 64, ... 중에서 max_trs 이하인 최대값 선정
    do
    {
        if ( max_trs > MAX_TRS ) MAX_TRS *= 2;
    }
    while ( ( 2 * MAX_TRS ) <= max_trs );

    //중간과 4-nearest neighbor의 좌표값 설정
    int trs_step_x[5] = {0, MAX_TRS,       0, -MAX_TRS,        0};
    int trs_step_y[5] = {0,       0, MAX_TRS,        0, -MAX_TRS};

    double mi[5], mi_max, pre_mi_max;
    int    mi_max_index, iter = 0;

    //match 영상용 객체 생성: crop과 크기 및 변수 동일
    image2D match( crop.yd, crop.xd, crop.scale, crop.bits_count );

    pre_mi_max = mi_max = 0.0;

    do
    {
        pre_mi_max = mi_max;
        mi_max_index = 0;
        for ( int i = 0; i < 5; i++ )
        {
            //타겟(this)에서 2D 변환된 match 영상 생성
            this->transformation2D( match, trs_y + trs_step_y[i], trs_x + trs_step_x[i] );
            //crop과 match의 mi 계산
            mi[i] = crop.mutualinformation( match, bins );
            //mi 최대방향 search
            if ( mi[i] > mi_max )
            {
                mi_max_index = i;
                mi_max = mi[i];
            }
        }
        //mi 최대 증가 방향으로 2D 변수 이동
        trs_y += trs_step_y[mi_max_index];
        trs_x += trs_step_x[mi_max_index];

        cout << iter << "-th iteration: mi = " << mi_max << "; dx=" << trs_step_x[1] << "; " << trs_x << " " << trs_y  << endl;

        iter++;

        //mi 변화가 없으면, step 변화폭을 1/2로 줄임
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

//두 영상을 stitching하여 raw 데이터를 파일에 저장
//mode = 0: 자연스럽게 stitching, 1: 타겟 강조, 2: 소스 강조
void	image2D::saving_stiching_result( image2D& source, int trans_y, int trans_x, char *filename, int mode )
{
    if ( ( trans_y < this->yd ) && ( trans_y >= 0 ) && ( trans_x > ( -this->xd / 5 ) ) && ( trans_x < ( this->xd / 5 ) ) )
    {


        int sum_yd, sum_xd;
        unsigned short temp = 0, v_t, v_s, val;
        double  r_t, r_s;

        sum_yd = this->yd + trans_y;  		  //stitching영상 크기
        sum_xd = this->xd + abs( trans_x );	 //stitching영상 크기

        cout << "합친 영상 : (" << sum_xd << ", " << sum_yd << ")" << endl;

        FILE *fp;
		errno_t error_fd; 
		error_fd = fopen_s(&fp, filename, "wb" );

        //타겟 영상중 소스 영상과 겹치지 않는 부분 저장
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
        //타겟 영상과 소스 영상이 겹치는 부분 자연스럽게 weight를 줘서 저장
        for ( int y = trans_y; y < this->yd; y++ )
        {
            r_t = ( y - trans_y ) / ( double )( this->yd - trans_y ); //never 0 on denominator (trans_y < this->yd)
            r_s = ( this->yd - y ) / ( double )( this->yd - trans_y ); //never 0

            if ( mode == 1 )
            {
                r_t = 0.0;    // 타겟 강조
                r_s = 1.0;
            }
            else if ( mode == 2 )
            {
                r_t = 1.0;    //소스 강조
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
        //소스영상중 타겟영상과 겹치지 않는 부분 저장
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

//타겟 영상과 소스영상을 매칭하는 주 알고리즘
//소스영상의 픽셀피치와 overlap 크기를 고려하여 매칭용 crop 영상 생성
//crop 영상 생성시 overlap이 60mm보다 크면 60mm에 해다하는 정도까지만 생성
//crop 영상은 축소해서 계산부담을 줄임
//최종으로 타겟 (0,0) 위치 대비 crop 영상의 매칭위치(trans_y, trans_x) 돌려줌
void image2D::stiching_2images( image2D& source, int& trans_y, int& trans_x, double pixel_pitch, double overlap )
{
    int crop_yd;

    //타겟 영상에서 픽셀값이 0인 경우 +1 shift: 픽셀이 0인 경우는 경계밖에 할당하고 mutual information 계산에서 제외
    this->lower_bounding_image_values( 1 );

    //소스 영상에서 매칭용 영상(crop) 가져오기: 높이 crop_yd
    if ( overlap < 60.0 ) crop_yd = ( int )( overlap / pixel_pitch ); // 최대 60mm 높이까지만 매칭에 사용
    else crop_yd = ( int )( 60.0 / pixel_pitch );
    if ( crop_yd > source.yd ) crop_yd = source.yd; // 혹시, crop 높이가 소스 높이보다 커지는 경우 억제
    image2D crop( crop_yd, source.xd, source.scale, source.bits_count );
    cout << "Crop 영상 : (" << crop.get_xd() << ", " << crop.get_yd() << "), scale = " << crop.get_scale() <<  endl;

    //crop영상 가져오기
    for ( int y = 0; y < crop_yd; y++ )
        for ( int x = 0; x < source.xd; x++ )
            crop.image[y][x] = source.image[y][x];

    //crop 영상 축소: scale=1,2,4,8,16, ....
    crop.image_shrink( 4 ); //USER OPT *************************.
    cout << "Crop 축소 : (" << crop.get_xd() << ", " << crop.get_yd() << "), scale = " << crop.get_scale() <<  endl;

    //crop 영상을 2D 이동하는 초기값 설정
    // 타겟 영상 하단과 소스영상 상단(crop) 중첩부의 겹침(overlap)을 대강 파악하여 타겟 하단에 crop부 맞춤
    trans_x = this->get_xd() / 2 - ( crop.get_xd() / 2 * crop.get_scale() );
    trans_y = this->get_yd()   - ( int )( overlap / pixel_pitch );

    cout << "초기 매칭 위치 : (" << trans_x << ", " << trans_y << ")" << endl;

    int 	max_trs = ( int )( 8.0 / pixel_pitch ); //USER OPT******.  매칭치 위치 파라미터 업데이트 step은 최대 8mm로 함 (경험치)
    int     iteration = 50;                       //USER OPT******. 매칭 알고리즘 반복수
    int     histogram_bins = 128;             //USER OPT******. mutual information 계산시 히스토그램 bin 수

    //주 매칭 함수: crop영상을 타겟영상에 매칭하고 매칭결과를(trans_y, trans_x) arguments로 리턴
    this->mi_gradient_matching( crop, trans_y, trans_x, max_trs, iteration, histogram_bins );

    cout << "최종 매칭 위치 : (" << trans_x << ", " << trans_y << ")" << endl;
}


#define IMAGE_YD		(int) 3008			//타겟 및 소스영상 높이
#define IMAGE_XD		(int) 3072			//타겟 및 소스영상 폭
#define IMAGE_PITCH		(double) 0.148		//타겟 및 소스영상 픽셀 피치 (mm)
#define IMAGE_BITS		(int) 16			//타겟 및 소스영상 비트수

#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>

int main( int argc, char **argv )
{
    
    	//testmain(argc,argv);
/*
		IMAGE * out;
    	int fd;
    	errno_t errno;
    	errno = _sopen_s(&fd, "teststitch2d.raw", _O_CREAT|_O_WRONLY|_O_BINARY|_O_TRUNC, _SH_DENYNO, _S_IREAD | _S_IWRITE);
    	if( !( out = im_open( "tmp.v", "t" ) ) )
            error_exit( "find_mosaic: unable to open file for output" );

    	im_raw2vips( "E:\\coreline-prj\\Stitch\\Stitch\\wholebody\\4-Knee.raw", out, IMAGE_XD, IMAGE_YD, 1, 0 );
    	im_vips2tiff( out, "test.tiff" );
    	im_vips2raw( out, fd );
    	im_close( out );
    	close(fd);
*/    
    
	if (strcmp(argv[1], "mosaic") != 0 &&
		strcmp(argv[1], "merge") != 0)
	{
		return 1;
	}

	if (strcmp(argv[1], "mosaic") == 0 )
	{
		char name[ 1024 ];
		int fd_list_point_file;
		errno_t err_no_pt_file;
		int number_points = 0;
		err_no_pt_file = _sopen_s(&fd_list_point_file, argv[2], _O_CREAT | _O_WRONLY | _O_TEXT |_O_TRUNC, 
																_SH_DENYNO, 
																_S_IREAD | _S_IWRITE);
		for (int i = 3; i < argc - 1; i++)
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

			err_no = _sopen_s(&fd, output_file_raw, _O_CREAT|_O_WRONLY|_O_BINARY|_O_TRUNC, _SH_DENYNO, _S_IREAD | _S_IWRITE);

			im_tiff2vips( argv[ i ], img_convert );
			im_vips2raw(img_convert, fd);

			FILE *fp;
			unsigned short *data;


			//타겟 영상 로딩: 1차원 unsigned short 배열에서 읽어들임
			//타겟 영상 하단(배열 후반부)에 아래의 소스영상 상단(배열 상반부)이 매칭되는 형태이어야 함
			data = new unsigned short[img_convert->Ysize * img_convert->Xsize];
			errno_t error_fd; 
			error_fd = fopen_s(&fp, output_file_raw, "rb" ); // "E:\\coreline-prj\\Stitch\\Stitch\\wholebody\\3-Hip.raw"
			if ( fp != 0 )
			{
				for ( int i = 0; i < img_convert->Ysize * img_convert->Xsize; i++ ) 
					fread( &data[i], sizeof( unsigned short ), 1, fp );
				cout << "타겟 영상 로딩: ";
			}
			else
			{
				cout << "타겟 영상 없음!\n";
				exit( 1 );
			}

			fclose( fp );

			//data를 이용하여 image2D 변수 target 생성
			image2D target( data, img_convert->Ysize, img_convert->Xsize, 1, IMAGE_BITS );
			cout << "(" << target.get_xd() << ", " << target.get_yd() << "), scale = " << target.get_scale() <<  endl;

			_close(fd);
			im_close(img_convert);

			file_root = remove_ext_tiff( argv[i + 1] );
			if( !file_root )
				error_exit( "error at file_root" );
			im_snprintf( name, 1024, "%s.v", file_root );
			output_file_v = _strdup( name );

			im_snprintf( name, 1024, "%s.raw", file_root );
			output_file_raw = _strdup( name );
			if( !( img_convert = im_open( output_file_v, "w" ) ) )
				error_exit( "find_mosaic: unable to open %s for input", output_file_v );

			err_no = _sopen_s(&fd, output_file_raw, _O_CREAT|_O_WRONLY|_O_BINARY|_O_TRUNC, _SH_DENYNO, _S_IREAD | _S_IWRITE);

			im_tiff2vips(argv[i + 1], img_convert);
			im_vips2raw(img_convert, fd);

			//소스 영상 로딩: 1차원 unsigned short 배열에서 읽어들임
			//타겟 영상 하단을 소스영상 상단이 매칭되는 형태이어야 함
			error_fd = fopen_s( &fp, output_file_raw, "rb" ); // "E:\\coreline-prj\\Stitch\\Stitch\\wholebody\\4-Knee.raw"
			if ( fp != 0 )
			{
				for ( int i = 0; i < img_convert->Ysize * img_convert->Xsize; i++ ) 
					fread( &data[i], sizeof( unsigned short ), 1, fp );
				cout << "소스 영상 로딩: ";
			}
			else
			{
				cout << "소스 영상 없음!\n";
				exit( 1 );
			}
			fclose( fp );
			//data를 이용하여 image2D 변수 source 생성
			image2D source( data, img_convert->Ysize, img_convert->Xsize, 1, IMAGE_BITS );
			cout << "(" << source.get_xd() << ", " << source.get_yd() << "), scale = " << source.get_scale() <<  endl;

			delete data;
			_close(fd);
			im_close(img_convert);

			int trans_x, trans_y;  // 영상 이동 좌표값
			double pixel_pitch = IMAGE_PITCH; //디텍터 픽셀 피치 (mm)
			double overlap = 60.0; //엑스선 촬영시 영상 오버랩의 눈대중 값 (mm): 실제값과 10~20mm 이상 차이나면 매칭이 어려움 // 60mm 추천
			int mode = 1;        // stitching 결과 저장 방식: 0 오버랩 부드럽게, 1 타겟 강조, 2 소스 강조
			target.stiching_2images( source, trans_y, trans_x, pixel_pitch, overlap );

			char points[ 28 ];
			char * output_point;
			memset(points, 0, 28);
			im_snprintf( points, 28, "%d %d\r\n", trans_x, trans_y );
			output_point = _strdup( points );
			_write( fd_list_point_file, output_point, strlen( output_point ) );
			number_points = i - 3;
			//target.saving_stiching_result( source, trans_y, trans_x, "sum.raw", mode );
		}

		char total_points[ 8 ];
		char * output_total_points;
		memset(total_points, 0, 8);
		im_snprintf( total_points, 8, "Number of Point: %d\r\n", number_points + 1 );
		output_total_points = _strdup( total_points );
		_write( fd_list_point_file, output_total_points, strlen( output_total_points ) );
		_close( fd_list_point_file );
		return 0;
	}


	if (strcmp(argv[1], "merge") == 0 )
	{
		int number_of_point = atoi(argv[2]);
		int k = 0;
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
			k++;

			IMAGE * sourceimg;
			IMAGE * targetimg;
			IMAGE * tmpOut;
			IMAGE * tmpOut1;
			IMAGE * tmpOut2;

			if ( _access( output_file_v, 0 ) == -1)
			{
				IMAGE * save_Out;
				if( !( save_Out = im_open( output_file_v , "w" ) ) ) // "E:\\coreline-prj\\Stitch\\Stitch\\wholebody\\3-Hip.v"
					error_exit( "stitch2d: unable to open file for output" );
				im_tiff2vips( argv[ i - 1 ], save_Out ); //, IMAGE_XD, IMAGE_YD, 1, 0 );
				im_close( save_Out );
			}

			if( !( sourceimg = im_open( output_file_v , "r" ) ) ) // "E:\\coreline-prj\\Stitch\\Stitch\\wholebody\\3-Hip.v"
			    error_exit( "stitch2d: unable to open file for output" );
			//im_tiff2vips( argv[i - 1], sourceimg ); //, IMAGE_XD, IMAGE_YD, 1, 0 );
			
			////////// SRC IMAGE ////////////

			char * directory = find_directory(argv[i]);				
			char * output_file_i_v; 
			im_snprintf( name, 1024, "%s\\temp__%d.v", directory, i + 1 );
			free(directory);
			output_file_i_v = _strdup( name );
			
			if ( _access( output_file_i_v, 0 ) != -1)
			{
				output_file_v = _strdup(name);
				if( !( targetimg = im_open( output_file_v, "r" ) ) )
					error_exit( "stitch2d: unable to open file for output" );
				//IMAGE * temporary_img;
				//temporary_img = im_open(output_file_i_v, "r");
				//im_copy( temporary_img, targetimg ); //, IMAGE_XD, IMAGE_YD, 1, 0 );
				//im_close(temporary_img);
			}
			else
			{

				file_root = remove_ext_tiff( argv[ i ] );
				if( !file_root )
					error_exit( "error at file_root" );
				im_snprintf( name, 1024, "%s.v", file_root );
				output_file_v = _strdup( name );

				if ( _access( output_file_v, 0 ) == -1)
				{
					IMAGE * save_Out;
					if( !( save_Out = im_open( output_file_v , "w" ) ) ) 
						error_exit( "stitch2d: unable to open file for output" );
					im_tiff2vips( argv[ i ], save_Out ); //, IMAGE_XD, IMAGE_YD, 1, 0 );
					im_close( save_Out );
				}
				if( !( targetimg = im_open( output_file_v, "r" ) ) )
					error_exit( "stitch2d: unable to open file for output" );
				//im_tiff2vips( argv[ i ], targetimg ); //, IMAGE_XD, IMAGE_YD, 1, 0 );
			}

			//im_vips2tiff(targetimg, "E:\\coreline-prj\\Stitch\\Stitch\\wholebody\\4-Knee.tiff");
	
			/*
				if( !( tmpOut2 = im_open( "E:\\coreline-prj\\Stitch\\Stitch\\wholebody\\4-Knee.v", "w" ) ) )
					error_exit( "stitch2d: unable to open file for output" );
				im_copy(targetimg , tmpOut2);
				im_close(tmpOut2);
			*/
	
			if( !( tmpOut = im_open( "tmpOut.v", "p" ) ) ||
			        !( tmpOut1 = im_open( "tmpOut1.v", "p" ) ) )
			    error_exit( "stitch2d: unable to open file for output" );
	
			im_tbmosaic( sourceimg, targetimg, tmpOut, 0, trans_x, trans_y, 0, 0, 5, 15, 0, 90 );
	
			//im_tbmerge(sourceimg, targetimg, tmpOut, 0 - trans_x, 0 - trans_y, 10 );
	
	//		im_tbmerge1(sourceimg, targetimg, tmpOut, trans_x, trans_y, trans_x + 500, trans_y, 0 , 0, 500, 0, 10 );
			/*
				if( !( tmpOut2 = im_open( "tmp3.v", "w" ) ) )
					error_exit( "stitch2d: unable to open file for output" );
				im_copy(tmpOut, tmpOut2);
				im_close(tmpOut2);
			*/
			directory = find_directory(argv[i]);
			im_snprintf( name, 1024, "%s\\temp__%d.v", directory, i );
			free(directory);
			output_file_i_v = _strdup( name );

			if( !( tmpOut2 = im_open( output_file_i_v, "w" ) ) )
			    error_exit( "stitch2d: unable to open file for output" );
			im_copy(tmpOut, tmpOut2);
			im_close(tmpOut2);
#if 0	
			im_global_balancef( tmpOut, tmpOut1, 0.8 );
#endif	
/*
			int fdOut;
			errno_t err;
			err = _sopen_s( &fdOut, "teststitchHIP_KNEE.raw", _O_CREAT | _O_WRONLY | _O_BINARY | _O_TRUNC, _SH_DENYNO, _S_IREAD | _S_IWRITE );
*/	
	
			im_vips2tiff( tmpOut, "teststitchHIP_KNEE_before_balance.tiff" );
			im_close( tmpOut );
	
			//im_vips2tiff( tmpOut1, "teststitchHIP_KNEE.tiff" );
	
	//		im_vips2raw( tmpOut1, fdOut );
	
			im_close( sourceimg );
			im_close( targetimg );
			im_close( tmpOut1 );

			free(output_file_i_v);
			free(output_file_v);
//			_close( fdOut );
		}
	}
    return 0;
}
