// vc_lc.inc - coordinates and zone names of the Vice City (vc2sa4samp) and
// Liberty City (lc2sa4samp) maps.
//
//   world = original game position + offset
//   Vice City:    + 6000.0 X, + 1050.0 Y, - 5.5 Z
//   Liberty City: - 5350.0 X, -  200.0 Y, + 0.0 Z
//
// Each city covers original -2000..2000 on both axes, so in world space:
//   Vice City:    X 4000..8000,      Y -950..3050
//   Liberty City: X -7350..-3350,    Y -2200..1800

#if defined _vc_lc_included
	#endinput
#endif
#define _vc_lc_included

#define VCLC_CITY_NONE          (0)
#define VCLC_CITY_VC            (1)
#define VCLC_CITY_LC            (2)

#define VCLC_VC_OFFSET_X        (6000.0)
#define VCLC_VC_OFFSET_Y        (1050.0)
#define VCLC_VC_OFFSET_Z        (-5.5)
#define VCLC_LC_OFFSET_X        (-5350.0)
#define VCLC_LC_OFFSET_Y        (-200.0)
#define VCLC_LC_OFFSET_Z        (0.0)
#define VCLC_CITY_HALF_SIZE     (2000.0)

enum E_VCLC_ZONE
{
	VCLC_ZONE_NAME[24],
	Float:VCLC_ZONE_AREA[6] // min x, min y, min z, max x, max y, max z
}

// Vice City: navig.zon, Liberty City: gta3.zon (info zones left out), moved by the offsets.
// First match wins, so smaller zones come before the big district boxes.
stock const VCLC_Zones[][E_VCLC_ZONE] =
{
	// Vice City
	{"Junkyard",                    {4603.240, 1007.089, -5.500, 4791.790, 1280.390, 294.500}},
	{"Downtown",                    {4386.970, 1463.128, -5.500, 5786.270, 2727.320, 294.500}},
	{"Vice Point",                  {6163.656, 698.847, -5.500, 7246.030, 2448.850, 294.500}},
	{"Washington Beach",            {5896.030, 119.474, -5.500, 7246.030, 698.847, 294.500}},
	{"Ocean Beach",                 {5746.794, -755.370, -5.500, 7254.900, 119.474, 294.500}},
	{"Escobar International",       {4111.790, -729.610, -5.500, 4791.790, 1280.390, 294.500}},
	{"Starfish Island",             {5251.794, 231.734, -5.500, 5895.495, 808.533, 294.500}},
	{"Prawn Island",                {5786.270, 1847.605, -5.500, 6163.656, 2293.470, 294.500}},
	{"Leaf Links",                  {5786.270, 808.571, -5.500, 6163.656, 1847.605, 294.500}},
	{"Viceport",                    {4791.790, -729.610, -5.500, 5746.794, 151.262, 294.500}},
	{"Little Havana",               {4791.790, 151.262, -5.500, 5251.794, 808.533, 294.500}},
	{"Little Haiti",                {4791.790, 808.533, -5.500, 5421.711, 1462.660, 294.500}},
	// Liberty City
	{"Callahan Bridge",             {-4732.558, -1158.347, 6.261, -4284.560, -1108.347, 206.261}},
	{"Callahan Bridge",             {-4905.232, -1158.298, 30.744, -4735.122, -1108.298, 180.744}},
	{"Callahan Point",              {-4598.320, -1378.220, -13.872, -4284.320, -1158.725, 136.128}},
	{"Atlantic Quays",              {-4284.120, -1451.550, -13.505, -3848.120, -1269.930, 136.495}},
	{"Portland Harbor",             {-3986.320, -1269.650, -18.864, -3534.320, -813.646, 131.136}},
	{"Trenton",                     {-4284.120, -1269.850, 1.499, -3986.620, -942.054, 151.499}},
	{"Chinatown",                   {-4604.579, -1108.289, -21.203, -4284.580, -663.690, 129.593}},
	{"Red Light District",          {-4604.622, -663.616, -22.668, -4284.620, -482.616, 147.332}},
	{"Hepburn Heights",             {-4604.579, -482.400, -13.412, -4284.580, -278.770, 136.588}},
	{"Saint Mark's",                {-4284.100, -712.324, -14.296, -3961.100, -278.324, 135.704}},
	{"Harwood",                     {-4604.021, -278.178, -48.583, -3961.020, 122.676, 101.417}},
	{"Portland Beach",              {-3960.630, -813.467, -29.883, -3552.400, -0.372, 120.117}},
	{"Portland View",               {-4283.900, -941.806, -34.207, -3986.400, -712.806, 115.793}},
	{"Fort Staunton",               {-5110.122, -611.617, 0.000, -4735.678, -261.617, 163.819}},
	{"Aspatria",                    {-5575.764, -612.604, 0.000, -5233.764, -39.504, 120.271}},
	{"Torrington",                  {-5150.234, -1872.420, -61.759, -4772.234, -1259.930, 432.688}},
	{"Bedford Point",               {-5574.438, -1872.050, -61.318, -5150.438, -1204.450, 432.352}},
	{"Newport",                     {-5149.893, -1259.190, 0.000, -4734.893, -612.193, 198.864}},
	{"Belleville Park",             {-5471.567, -1203.070, -46.746, -5150.729, -613.068, 224.163}},
	{"Liberty Campus",              {-5232.732, -611.622, 0.000, -5110.732, -261.622, 166.360}},
	{"Rockford",                    {-5232.764, -261.111, -17.071, -4734.764, 68.889, 83.754}},
	{"Francis Intl. Airport",       {-6982.970, -1544.710, -45.940, -5818.629, -468.443, 254.696}},
	{"Wichita Gardens",             {-6161.835, -468.074, -45.874, -5721.041, -107.274, 254.241}},
	{"Cedar Grove",                 {-6217.229, -106.612, -50.113, -5616.914, 450.058, 250.426}},
	{"Pike Creek",                  {-6757.570, -467.966, -49.679, -6162.306, -107.244, 250.437}},
	{"Cochrane Dam",                {-6744.500, -106.556, -46.741, -6217.520, 504.544, 253.344}},
	{"Shoreside Vale",              {-5615.444, -38.887, -41.709, -5471.287, 167.043, 358.291}},
	{"Shoreside Vale",              {-5615.434, -120.908, -45.820, -5576.334, -38.936, 354.180}},
	{"Portland",                    {-4732.849, -1529.720, -117.535, -3447.340, 234.115, 482.465}},
	{"Staunton Island",             {-5615.479, -1919.970, -114.769, -4734.480, 167.265, 485.231}},
	{"Shoreside Vale",              {-6994.640, -1551.380, -117.000, -5616.895, 1006.350, 483.000}}
};

// VCLC_CITY_VC, VCLC_CITY_LC or VCLC_CITY_NONE (San Andreas or anywhere else)
stock VCLC_GetCity(Float:x, Float:y)
{
	if(floatabs(x - VCLC_VC_OFFSET_X) <= VCLC_CITY_HALF_SIZE && floatabs(y - VCLC_VC_OFFSET_Y) <= VCLC_CITY_HALF_SIZE) return VCLC_CITY_VC;
	if(floatabs(x - VCLC_LC_OFFSET_X) <= VCLC_CITY_HALF_SIZE && floatabs(y - VCLC_LC_OFFSET_Y) <= VCLC_CITY_HALF_SIZE) return VCLC_CITY_LC;
	return VCLC_CITY_NONE;
}

stock VCLC_GetCityName(city, name[], len = sizeof(name))
{
	switch(city)
	{
		case VCLC_CITY_VC: return strcopy(name, "Vice City", len);
		case VCLC_CITY_LC: return strcopy(name, "Liberty City", len);
	}
	return strcopy(name, "San Andreas", len);
}

// zone name at a world position; falls back to the city name, returns 0 outside both cities
stock VCLC_GetZoneName(Float:x, Float:y, Float:z, name[], len = sizeof(name))
{
	for(new i = 0; i < sizeof(VCLC_Zones); i++)
	{
		if(x >= VCLC_Zones[i][VCLC_ZONE_AREA][0] && x <= VCLC_Zones[i][VCLC_ZONE_AREA][3] &&
			y >= VCLC_Zones[i][VCLC_ZONE_AREA][1] && y <= VCLC_Zones[i][VCLC_ZONE_AREA][4] &&
			z >= VCLC_Zones[i][VCLC_ZONE_AREA][2] && z <= VCLC_Zones[i][VCLC_ZONE_AREA][5])
		{
			strcopy(name, VCLC_Zones[i][VCLC_ZONE_NAME], len);
			return 1;
		}
	}
	new city = VCLC_GetCity(x, y);
	if(city == VCLC_CITY_NONE)
	{
		name[0] = EOS;
		return 0;
	}
	VCLC_GetCityName(city, name, len);
	return 1;
}

// original Vice City / GTA III position -> SA-MP world position
stock VCLC_FromOriginal(city, Float:ox, Float:oy, Float:oz, &Float:x, &Float:y, &Float:z)
{
	switch(city)
	{
		case VCLC_CITY_VC: { x = ox + VCLC_VC_OFFSET_X; y = oy + VCLC_VC_OFFSET_Y; z = oz + VCLC_VC_OFFSET_Z; }
		case VCLC_CITY_LC: { x = ox + VCLC_LC_OFFSET_X; y = oy + VCLC_LC_OFFSET_Y; z = oz + VCLC_LC_OFFSET_Z; }
		default: return 0;
	}
	return 1;
}

// SA-MP world position -> original Vice City / GTA III position
stock VCLC_ToOriginal(Float:x, Float:y, Float:z, &Float:ox, &Float:oy, &Float:oz)
{
	switch(VCLC_GetCity(x, y))
	{
		case VCLC_CITY_VC: { ox = x - VCLC_VC_OFFSET_X; oy = y - VCLC_VC_OFFSET_Y; oz = z - VCLC_VC_OFFSET_Z; }
		case VCLC_CITY_LC: { ox = x - VCLC_LC_OFFSET_X; oy = y - VCLC_LC_OFFSET_Y; oz = z - VCLC_LC_OFFSET_Z; }
		default: return 0;
	}
	return 1;
}
