#include "mt_partition.h"

part_t partition_layout[] = {
	{PART_PRELOADER, PART_BLKS_PRELOADER, PART_FLAG_NONE,0},
	{PART_MBR, PART_BLKS_MBR, PART_FLAG_NONE,0},
	{PART_EBR1, PART_BLKS_EBR1, PART_FLAG_NONE,0},
	{PART_PMT, PART_BLKS_PMT, PART_FLAG_NONE,0},
	{PART_PRO_INFO, PART_BLKS_PRO_INFO, PART_FLAG_NONE,0},
	{PART_NVRAM, PART_BLKS_NVRAM, PART_FLAG_NONE,0},
	{PART_PROTECT_F, PART_BLKS_PROTECT_F, PART_FLAG_NONE,0},
	{PART_PROTECT_S, PART_BLKS_PROTECT_S, PART_FLAG_NONE,0},
	{PART_SECURE, PART_BLKS_SECURE, 0, PART_FLAG_NONE},
	{PART_UBOOT, PART_BLKS_UBOOT, PART_FLAG_NONE,0},
	{PART_BOOTIMG, PART_BLKS_BOOTIMG, PART_FLAG_NONE,0},
	{PART_RECOVERY, PART_BLKS_RECOVERY, PART_FLAG_NONE,0},
	{PART_SECSTATIC, PART_BLKS_SECSTATIC, 0, PART_FLAG_NONE},
	{PART_MISC, PART_BLKS_MISC, PART_FLAG_NONE,0},
	{PART_LOGO, PART_BLKS_LOGO, PART_FLAG_NONE,0},
	{PART_APANIC, PART_BLKS_APANIC, 0, PART_FLAG_NONE},
	{PART_ANDSYSIMG, PART_BLKS_ANDSYSIMG, 0, PART_FLAG_NONE},
	{PART_CACHE, PART_BLKS_CACHE, PART_FLAG_NONE,0},
	{PART_USER, PART_BLKS_USER, 0, PART_FLAG_NONE},
	{PART_FAT, PART_BLKS_FAT, PART_FLAG_NONE,0},
	{NULL, 0, PART_FLAG_END, 0},
};

struct part_name_map g_part_name_map[PART_MAX_COUNT] = {
	{"preloader",	PART_PRELOADER,	"raw data",	0,	0,	0},
	{"mbr",	PART_MBR,	"raw data",	1,	0,	0},
	{"ebr1",	PART_EBR1,	"raw data",	2,	0,	0},
	{"pmt",	PART_PMT,	"raw data",	3,	0,	0},
	{"pro_info",	PART_PRO_INFO,	"raw data",	4,	0,	0},
	{"nvram",	PART_NVRAM,	"raw data",	5,	0,	0},
	{"protect_f",	PART_PROTECT_F,	"ext4",	6,	0,	0},
	{"protect_s",	PART_PROTECT_S,	"ext4",	7,	0,	0},
	{"seccfg",	PART_SECURE,	"raw data",	8,	0,	0},
	{"uboot",	PART_UBOOT,	"raw data",	9,	0,	0},
	{"boot",	PART_BOOTIMG,	"raw data",	10,	1,	1},
	{"recovery",	PART_RECOVERY,	"raw data",	11,	0,	0},
	{"sec_ro",	PART_SECSTATIC,	"raw data",	12,	0,	0},
	{"misc",	PART_MISC,	"raw data",	13,	0,	0},
	{"logo",	PART_LOGO,	"raw data",	14,	0,	0},
	{"expdb",	PART_APANIC,	"raw data",	15,	0,	0},
	{"system",	PART_ANDSYSIMG,	"ext4",	16,	1,	1},
	{"cache",	PART_CACHE,	"ext4",	17,	1,	1},
	{"userdata",	PART_USER,	"ext4",	18,	1,	1},
	{"fat",	PART_FAT,	"fat",	19,	0,	0},
};
