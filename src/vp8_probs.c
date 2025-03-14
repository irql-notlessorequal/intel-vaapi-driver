/*
 * Copyright (C) 2014-2017 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/*
 * This file defines some vp8 probability tables, and
 * they are ported from libvpx (https://github.com/mrchapp/libvpx/).
 * The original copyright and licence statement as below.
 */

/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

const unsigned char vp8_ymode_prob[4] = {
	112, 86, 140, 37
};

const unsigned char vp8_kf_ymode_prob[4] = {
	145, 156, 163, 128
};

const unsigned char vp8_uv_mode_prob[3] = {
	162, 101, 204
};

const unsigned char vp8_kf_uv_mode_prob[3] = {
	142, 114, 183
};

const unsigned char vp8_base_skip_false_prob[128] = {
	255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255,
	251, 248, 244, 240, 236, 232, 229, 225,
	221, 217, 213, 208, 204, 199, 194, 190,
	187, 183, 179, 175, 172, 168, 164, 160,
	157, 153, 149, 145, 142, 138, 134, 130,
	127, 124, 120, 117, 114, 110, 107, 104,
	101, 98,  95,  92,  89,  86,  83, 80,
	77,  74,  71,  68,  65,  62,  59, 56,
	53,  50,  47,  44,  41,  38,  35, 32,
	30,  28,  26,  24,  22,  20,  18, 16,
};

const unsigned char vp8_mv_update_probs[2][19] = {
	{
		237,
		246,
		253, 253, 254, 254, 254, 254, 254,
		254, 254, 254, 254, 254, 250, 250, 252, 254, 254
	},
	{
		231,
		243,
		245, 253, 254, 254, 254, 254, 254,
		254, 254, 254, 254, 254, 251, 251, 254, 254, 254
	}
};

const unsigned char vp8_default_mv_context[2][19] = {
	{
		162,                                        /* is short */
		128,                                        /* sign */
		225, 146, 172, 147, 214,  39, 156,          /* short tree */
		128, 129, 132,  75, 145, 178, 206, 239, 254, 254 /* long bits */
	},

	{
		164,
		128,
		204, 170, 119, 235, 140, 230, 228,
		128, 130, 130,  74, 148, 180, 203, 236, 254, 254

	}
};

const unsigned char vp8_default_coef_probs[4][8][3][11] = {
	{ /* Block Type ( 0 ) */
		{ /* Coeff Band ( 0 )*/
			{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 },
			{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 },
			{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 }
		},
		{ /* Coeff Band ( 1 )*/
			{ 253, 136, 254, 255, 228, 219, 128, 128, 128, 128, 128 },
			{ 189, 129, 242, 255, 227, 213, 255, 219, 128, 128, 128 },
			{ 106, 126, 227, 252, 214, 209, 255, 255, 128, 128, 128 }
		},
		{ /* Coeff Band ( 2 )*/
			{   1,  98, 248, 255, 236, 226, 255, 255, 128, 128, 128 },
			{ 181, 133, 238, 254, 221, 234, 255, 154, 128, 128, 128 },
			{  78, 134, 202, 247, 198, 180, 255, 219, 128, 128, 128 }
		},
		{ /* Coeff Band ( 3 )*/
			{   1, 185, 249, 255, 243, 255, 128, 128, 128, 128, 128 },
			{ 184, 150, 247, 255, 236, 224, 128, 128, 128, 128, 128 },
			{  77, 110, 216, 255, 236, 230, 128, 128, 128, 128, 128 }
		},
		{ /* Coeff Band ( 4 )*/
			{   1, 101, 251, 255, 241, 255, 128, 128, 128, 128, 128 },
			{ 170, 139, 241, 252, 236, 209, 255, 255, 128, 128, 128 },
			{  37, 116, 196, 243, 228, 255, 255, 255, 128, 128, 128 }
		},
		{ /* Coeff Band ( 5 )*/
			{   1, 204, 254, 255, 245, 255, 128, 128, 128, 128, 128 },
			{ 207, 160, 250, 255, 238, 128, 128, 128, 128, 128, 128 },
			{ 102, 103, 231, 255, 211, 171, 128, 128, 128, 128, 128 }
		},
		{ /* Coeff Band ( 6 )*/
			{   1, 152, 252, 255, 240, 255, 128, 128, 128, 128, 128 },
			{ 177, 135, 243, 255, 234, 225, 128, 128, 128, 128, 128 },
			{  80, 129, 211, 255, 194, 224, 128, 128, 128, 128, 128 }
		},
		{ /* Coeff Band ( 7 )*/
			{   1,   1, 255, 128, 128, 128, 128, 128, 128, 128, 128 },
			{ 246,   1, 255, 128, 128, 128, 128, 128, 128, 128, 128 },
			{ 255, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 }
		}
	},
	{ /* Block Type ( 1 ) */
		{ /* Coeff Band ( 0 )*/
			{ 198,  35, 237, 223, 193, 187, 162, 160, 145, 155,  62 },
			{ 131,  45, 198, 221, 172, 176, 220, 157, 252, 221,   1 },
			{  68,  47, 146, 208, 149, 167, 221, 162, 255, 223, 128 }
		},
		{ /* Coeff Band ( 1 )*/
			{   1, 149, 241, 255, 221, 224, 255, 255, 128, 128, 128 },
			{ 184, 141, 234, 253, 222, 220, 255, 199, 128, 128, 128 },
			{  81,  99, 181, 242, 176, 190, 249, 202, 255, 255, 128 }
		},
		{ /* Coeff Band ( 2 )*/
			{   1, 129, 232, 253, 214, 197, 242, 196, 255, 255, 128 },
			{  99, 121, 210, 250, 201, 198, 255, 202, 128, 128, 128 },
			{  23,  91, 163, 242, 170, 187, 247, 210, 255, 255, 128 }
		},
		{ /* Coeff Band ( 3 )*/
			{   1, 200, 246, 255, 234, 255, 128, 128, 128, 128, 128 },
			{ 109, 178, 241, 255, 231, 245, 255, 255, 128, 128, 128 },
			{  44, 130, 201, 253, 205, 192, 255, 255, 128, 128, 128 }
		},
		{ /* Coeff Band ( 4 )*/
			{   1, 132, 239, 251, 219, 209, 255, 165, 128, 128, 128 },
			{  94, 136, 225, 251, 218, 190, 255, 255, 128, 128, 128 },
			{  22, 100, 174, 245, 186, 161, 255, 199, 128, 128, 128 }
		},
		{ /* Coeff Band ( 5 )*/
			{   1, 182, 249, 255, 232, 235, 128, 128, 128, 128, 128 },
			{ 124, 143, 241, 255, 227, 234, 128, 128, 128, 128, 128 },
			{  35,  77, 181, 251, 193, 211, 255, 205, 128, 128, 128 }
		},
		{ /* Coeff Band ( 6 )*/
			{   1, 157, 247, 255, 236, 231, 255, 255, 128, 128, 128 },
			{ 121, 141, 235, 255, 225, 227, 255, 255, 128, 128, 128 },
			{  45,  99, 188, 251, 195, 217, 255, 224, 128, 128, 128 }
		},
		{ /* Coeff Band ( 7 )*/
			{   1,   1, 251, 255, 213, 255, 128, 128, 128, 128, 128 },
			{ 203,   1, 248, 255, 255, 128, 128, 128, 128, 128, 128 },
			{ 137,   1, 177, 255, 224, 255, 128, 128, 128, 128, 128 }
		}
	},
	{ /* Block Type ( 2 ) */
		{ /* Coeff Band ( 0 )*/
			{ 253,   9, 248, 251, 207, 208, 255, 192, 128, 128, 128 },
			{ 175,  13, 224, 243, 193, 185, 249, 198, 255, 255, 128 },
			{  73,  17, 171, 221, 161, 179, 236, 167, 255, 234, 128 }
		},
		{ /* Coeff Band ( 1 )*/
			{   1,  95, 247, 253, 212, 183, 255, 255, 128, 128, 128 },
			{ 239,  90, 244, 250, 211, 209, 255, 255, 128, 128, 128 },
			{ 155,  77, 195, 248, 188, 195, 255, 255, 128, 128, 128 }
		},
		{ /* Coeff Band ( 2 )*/
			{   1,  24, 239, 251, 218, 219, 255, 205, 128, 128, 128 },
			{ 201,  51, 219, 255, 196, 186, 128, 128, 128, 128, 128 },
			{  69,  46, 190, 239, 201, 218, 255, 228, 128, 128, 128 }
		},
		{ /* Coeff Band ( 3 )*/
			{   1, 191, 251, 255, 255, 128, 128, 128, 128, 128, 128 },
			{ 223, 165, 249, 255, 213, 255, 128, 128, 128, 128, 128 },
			{ 141, 124, 248, 255, 255, 128, 128, 128, 128, 128, 128 }
		},
		{ /* Coeff Band ( 4 )*/
			{   1,  16, 248, 255, 255, 128, 128, 128, 128, 128, 128 },
			{ 190,  36, 230, 255, 236, 255, 128, 128, 128, 128, 128 },
			{ 149,   1, 255, 128, 128, 128, 128, 128, 128, 128, 128 }
		},
		{ /* Coeff Band ( 5 )*/
			{   1, 226, 255, 128, 128, 128, 128, 128, 128, 128, 128 },
			{ 247, 192, 255, 128, 128, 128, 128, 128, 128, 128, 128 },
			{ 240, 128, 255, 128, 128, 128, 128, 128, 128, 128, 128 }
		},
		{ /* Coeff Band ( 6 )*/
			{   1, 134, 252, 255, 255, 128, 128, 128, 128, 128, 128 },
			{ 213,  62, 250, 255, 255, 128, 128, 128, 128, 128, 128 },
			{  55,  93, 255, 128, 128, 128, 128, 128, 128, 128, 128 }
		},
		{ /* Coeff Band ( 7 )*/
			{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 },
			{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 },
			{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 }
		}
	},
	{ /* Block Type ( 3 ) */
		{ /* Coeff Band ( 0 )*/
			{ 202,  24, 213, 235, 186, 191, 220, 160, 240, 175, 255 },
			{ 126,  38, 182, 232, 169, 184, 228, 174, 255, 187, 128 },
			{  61,  46, 138, 219, 151, 178, 240, 170, 255, 216, 128 }
		},
		{ /* Coeff Band ( 1 )*/
			{   1, 112, 230, 250, 199, 191, 247, 159, 255, 255, 128 },
			{ 166, 109, 228, 252, 211, 215, 255, 174, 128, 128, 128 },
			{  39,  77, 162, 232, 172, 180, 245, 178, 255, 255, 128 }
		},
		{ /* Coeff Band ( 2 )*/
			{   1,  52, 220, 246, 198, 199, 249, 220, 255, 255, 128 },
			{ 124,  74, 191, 243, 183, 193, 250, 221, 255, 255, 128 },
			{  24,  71, 130, 219, 154, 170, 243, 182, 255, 255, 128 }
		},
		{ /* Coeff Band ( 3 )*/
			{   1, 182, 225, 249, 219, 240, 255, 224, 128, 128, 128 },
			{ 149, 150, 226, 252, 216, 205, 255, 171, 128, 128, 128 },
			{  28, 108, 170, 242, 183, 194, 254, 223, 255, 255, 128 }
		},
		{ /* Coeff Band ( 4 )*/
			{   1,  81, 230, 252, 204, 203, 255, 192, 128, 128, 128 },
			{ 123, 102, 209, 247, 188, 196, 255, 233, 128, 128, 128 },
			{  20,  95, 153, 243, 164, 173, 255, 203, 128, 128, 128 }
		},
		{ /* Coeff Band ( 5 )*/
			{   1, 222, 248, 255, 216, 213, 128, 128, 128, 128, 128 },
			{ 168, 175, 246, 252, 235, 205, 255, 255, 128, 128, 128 },
			{  47, 116, 215, 255, 211, 212, 255, 255, 128, 128, 128 }
		},
		{ /* Coeff Band ( 6 )*/
			{   1, 121, 236, 253, 212, 214, 255, 255, 128, 128, 128 },
			{ 141,  84, 213, 252, 201, 202, 255, 219, 128, 128, 128 },
			{  42,  80, 160, 240, 162, 185, 255, 205, 128, 128, 128 }
		},
		{ /* Coeff Band ( 7 )*/
			{   1,   1, 255, 128, 128, 128, 128, 128, 128, 128, 128 },
			{ 244,   1, 255, 128, 128, 128, 128, 128, 128, 128, 128 },
			{ 238,   1, 255, 128, 128, 128, 128, 128, 128, 128, 128 }
		}
	}
};

/* Work in progress recalibration of baseline rate tables based on
 * the assumption that bits per mb is inversely proportional to the
 * quantizer value.
 * Note: this table value multiplied by 512
 */
const int vp8_bits_per_mb[2][128] = {
	/* Intra case 450000/Qintra */
	{
		1125000, 900000, 750000, 642857, 562500, 500000, 450000, 450000,
		409090, 375000, 346153, 321428, 300000, 281250, 264705, 264705,
		250000, 236842, 225000, 225000, 214285, 214285, 204545, 204545,
		195652, 195652, 187500, 180000, 180000, 173076, 166666, 160714,
		155172, 150000, 145161, 140625, 136363, 132352, 128571, 125000,
		121621, 121621, 118421, 115384, 112500, 109756, 107142, 104651,
		102272, 100000, 97826,  97826,  95744,  93750,  91836,  90000,
		88235,  86538,  84905,  83333,  81818,  80357,  78947,  77586,
		76271,  75000,  73770,  72580,  71428,  70312,  69230,  68181,
		67164,  66176,  65217,  64285,  63380,  62500,  61643,  60810,
		60000,  59210,  59210,  58441,  57692,  56962,  56250,  55555,
		54878,  54216,  53571,  52941,  52325,  51724,  51136,  50561,
		49450,  48387,  47368,  46875,  45918,  45000,  44554,  44117,
		43269,  42452,  41666,  40909,  40178,  39473,  38793,  38135,
		36885,  36290,  35714,  35156,  34615,  34090,  33582,  33088,
		32608,  32142,  31468,  31034,  30405,  29801,  29220,  28662,
	},

	/* Inter case 285000/Qinter */
	{
		712500, 570000, 475000, 407142, 356250, 316666, 285000, 259090,
		237500, 219230, 203571, 190000, 178125, 167647, 158333, 150000,
		142500, 135714, 129545, 123913, 118750, 114000, 109615, 105555,
		101785, 98275,  95000,  91935,  89062,  86363,  83823,  81428,
		79166,  77027,  75000,  73076,  71250,  69512,  67857,  66279,
		64772,  63333,  61956,  60638,  59375,  58163,  57000,  55882,
		54807,  53773,  52777,  51818,  50892,  50000,  49137,  47500,
		45967,  44531,  43181,  41911,  40714,  39583,  38513,  37500,
		36538,  35625,  34756,  33928,  33139,  32386,  31666,  30978,
		30319,  29687,  29081,  28500,  27941,  27403,  26886,  26388,
		25909,  25446,  25000,  24568,  23949,  23360,  22800,  22265,
		21755,  21268,  20802,  20357,  19930,  19520,  19127,  18750,
		18387,  18037,  17701,  17378,  17065,  16764,  16473,  16101,
		15745,  15405,  15079,  14766,  14467,  14179,  13902,  13636,
		13380,  13133,  12895,  12666,  12445,  12179,  11924,  11632,
		11445,  11220,  11003,  10795,  10594,  10401,  10215,  10035,
	}
};

const unsigned short vp8_prob_cost[256] = {
	2047, 2047, 1791, 1641, 1535, 1452, 1385, 1328, 1279, 1235, 1196, 1161, 1129, 1099, 1072, 1046,
	1023, 1000,  979,  959,  940,  922,  905,  889,  873,  858,  843,  829,  816,  803,  790,  778,
	767,  755,  744,  733,  723,  713,  703,  693,  684,  675,  666,  657,  649,  641,  633,  625,
	617,  609,  602,  594,  587,  580,  573,  567,  560,  553,  547,  541,  534,  528,  522,  516,
	511,  505,  499,  494,  488,  483,  477,  472,  467,  462,  457,  452,  447,  442,  437,  433,
	428,  424,  419,  415,  410,  406,  401,  397,  393,  389,  385,  381,  377,  373,  369,  365,
	361,  357,  353,  349,  346,  342,  338,  335,  331,  328,  324,  321,  317,  314,  311,  307,
	304,  301,  297,  294,  291,  288,  285,  281,  278,  275,  272,  269,  266,  263,  260,  257,
	255,  252,  249,  246,  243,  240,  238,  235,  232,  229,  227,  224,  221,  219,  216,  214,
	211,  208,  206,  203,  201,  198,  196,  194,  191,  189,  186,  184,  181,  179,  177,  174,
	172,  170,  168,  165,  163,  161,  159,  156,  154,  152,  150,  148,  145,  143,  141,  139,
	137,  135,  133,  131,  129,  127,  125,  123,  121,  119,  117,  115,  113,  111,  109,  107,
	105,  103,  101,   99,   97,   95,   93,   92,   90,   88,   86,   84,   82,   81,   79,   77,
	75,   73,   72,   70,   68,   66,   65,   63,   61,   60,   58,   56,   55,   53,   51,   50,
	48,   46,   45,   43,   41,   40,   38,   37,   35,   33,   32,   30,   29,   27,   25,   24,
	22,   21,   19,   18,   16,   15,   13,   12,   10,    9,    7,    6,    4,    3,    1,   1
};

const unsigned char vp8_coef_update_probs[4][8][3][11] = {
	{
		{
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{176, 246, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{223, 241, 252, 255, 255, 255, 255, 255, 255, 255, 255, },
			{249, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 244, 252, 255, 255, 255, 255, 255, 255, 255, 255, },
			{234, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 246, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{239, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{254, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 248, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{251, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{251, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{254, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 254, 253, 255, 254, 255, 255, 255, 255, 255, 255, },
			{250, 255, 254, 255, 254, 255, 255, 255, 255, 255, 255, },
			{254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
	},
	{
		{
			{217, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{225, 252, 241, 253, 255, 255, 254, 255, 255, 255, 255, },
			{234, 250, 241, 250, 253, 255, 253, 254, 255, 255, 255, },
		},
		{
			{255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{223, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{238, 253, 254, 254, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 248, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{249, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 253, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{247, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{252, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 254, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
			{250, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
	},
	{
		{
			{186, 251, 250, 255, 255, 255, 255, 255, 255, 255, 255, },
			{234, 251, 244, 254, 255, 255, 255, 255, 255, 255, 255, },
			{251, 251, 243, 253, 254, 255, 254, 255, 255, 255, 255, },
		},
		{
			{255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{236, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{251, 253, 253, 254, 254, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
	},
	{
		{
			{248, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{250, 254, 252, 254, 255, 255, 255, 255, 255, 255, 255, },
			{248, 254, 249, 253, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
			{246, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
			{252, 254, 251, 254, 254, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 254, 252, 255, 255, 255, 255, 255, 255, 255, 255, },
			{248, 254, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
			{253, 255, 254, 254, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 251, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{245, 251, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{253, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 251, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
			{252, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 252, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{249, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 255, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
			{250, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
		{
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
			{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
		},
	},
};

const unsigned char vp8_probs_update_flag[4][8][3][11] = {
	{
		{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
		{{1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0}},
		{{0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}},
		{{0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}},
		{{0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}},
		{{0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}},
		{{0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}},
		{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
	},
	{
		{{1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
		{{0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0}},
		{{0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
		{{0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
		{{0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
		{{0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
		{{0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
		{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
	},
	{
		{{1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
		{{0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
		{{0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
		{{0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
		{{0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
		{{0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
		{{0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
		{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
	},
	{
		{{1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
		{{0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0}},
		{{0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
		{{0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
		{{0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
		{{0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
		{{0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
		{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}
	}
};
