#include <vconf.h>
#include <drm-service.h>

#include "media-server-utils.h"
#include "media-server-inotify.h"
#include "media-server-external-storage.h"

#define MMC_INFO_SIZE 256
#define DIR_NUM 5

int mmc_state = 0;
extern int current_usb_mode;
extern GAsyncQueue *scan_queue;

void
ms_make_default_path_mmc(void)
{
	MS_DBG_START();

	int i = 0;
	int ret = 0;
	DIR *dp = NULL;

	char default_path[DIR_NUM][MS_FILE_NAME_LEN_MAX + 1] = {
		{"/opt/storage/sdcard/Images"},
		{"/opt/storage/sdcard/Videos"},
		{"/opt/storage/sdcard/Music"},
		{"/opt/storage/sdcard/Downloads"},
		{"/opt/storage/sdcard/Camera shots"}
	};

	for (i = 0; i < DIR_NUM; ++i) {
		dp = opendir(default_path[i]);
		if (dp == NULL) {
			ret = mkdir(default_path[i], 0777);
			if (ret < 0) {
				MS_DBG("make fail");
			} else {
				ms_inoti_add_watch(default_path[i]);
			}
		} else {
			closedir(dp);
		}
	}

	MS_DBG_END();
}

int
_ms_update_mmc_info(const char *cid)
{
	MS_DBG_START();
	bool res;

	if (cid == NULL) {
		MS_DBG("Parameters are invalid");
		return MS_ERR_ARG_INVALID;
	}

	res = ms_config_set_str(MS_MMC_INFO_KEY, cid);
	if (!res) {
		MS_DBG("fail to get MS_MMC_INFO_KEY");
		return MS_ERR_VCONF_SET_FAIL;
	}

	MS_DBG_END();

	return MS_ERR_NONE;
}

bool
_ms_check_mmc_info(const char *cid)
{
	MS_DBG_START();

	char pre_mmc_info[MMC_INFO_SIZE] = { 0 };
	bool res = false;

	if (cid == NULL) {
		MS_DBG("Parameters are invalid");
		return false;
	}

	res = ms_config_get_str(MS_MMC_INFO_KEY, pre_mmc_info);
	if (!res) {
		MS_DBG("fail to get MS_MMC_INFO_KEY");
		return false;
	}

	MS_DBG("Last MMC info	= %s", pre_mmc_info);
	MS_DBG("Current MMC info = %s", cid);

	if (strcmp(pre_mmc_info, cid) == 0) {
		return true;
	}

	MS_DBG_END();
	return false;
}

static int
_get_contents(const char *filename, char *buf)
{
	FILE *fp;

	MS_DBG("%s", filename);

	fp = fopen(filename, "rt");
	if (fp == NULL) {
		MS_DBG("fp is NULL");
		return MS_ERR_FILE_OPEN_FAIL;
	}
	fgets(buf, 255, fp);

	fclose(fp);

	return MS_ERR_NONE;
}

/*need optimize*/
int
_ms_get_mmc_info(char *cid)
{
	MS_DBG_START();

	int i;
	int j;
	int len;
	int err = -1;
	bool getdata = false;
	bool bHasColon = false;
	char path[MS_FILE_PATH_LEN_MAX] = { 0 };
	char mmcpath[MS_FILE_PATH_LEN_MAX] = { 0 };

	DIR *dp;
	struct dirent ent;
	struct dirent *res = NULL;

	/* mmcblk0 and mmcblk1 is reserved for movinand */
	for (j = 1; j < 3; j++) {
		len = snprintf(mmcpath, MS_FILE_PATH_LEN_MAX, "/sys/class/mmc_host/mmc%d/", j);
		if (len < 0) {
			MS_DBG("FAIL : snprintf");
			return MS_ERR_UNKNOWN_ERROR;
		}
		else {
			mmcpath[len] = '\0';
		}

		dp = opendir(mmcpath);

		if (dp == NULL) {
			MS_DBG("dp is NULL");
			{
				return MS_ERR_DIR_OPEN_FAIL;
			}
		}

		while (!readdir_r(dp, &ent, &res)) {
			 /*end of read dir*/
			if (res == NULL)
				break;

			bHasColon = false;
			if (ent.d_name[0] == '.')
				continue;

			if (ent.d_type == DT_DIR) {
				/*ent->d_name is including ':' */
				for (i = 0; i < strlen(ent.d_name); i++) {
					if (ent.d_name[i] == ':') {
						bHasColon = true;
						break;
					}
				}

				if (bHasColon) {
					/*check serial */
					err = ms_strappend(path, sizeof(path), "%s%s/cid", mmcpath, ent.d_name);
					if (err < 0) {
						MS_DBG("FAIL : ms_strappend");
						continue;
					}

					if (_get_contents(path, cid) != MS_ERR_NONE)
						break;
					else
						getdata = true;
					MS_DBG("MMC serial : %s", cid);
				}
			}
		}
		closedir(dp);

		if (getdata == true) {
			break;
		}
	}

	MS_DBG_END();

	return MS_ERR_NONE;
}

ms_dir_scan_type_t
ms_get_mmc_state(void)
{
	char cid[MMC_INFO_SIZE] = { 0 };
	ms_dir_scan_type_t ret = MS_SCAN_ALL;

	/*get new info */
	_ms_get_mmc_info(cid);

	/*check it's same mmc */
	if (_ms_check_mmc_info(cid)) {
		MS_DBG("Detected same MMC! but needs to update the changes...");
		ret = MS_SCAN_PART;
	}

	return ret;
}

int
ms_update_mmc_info(void)
{
	int err;
	char cid[MMC_INFO_SIZE] = { 0 };

	err = _ms_get_mmc_info(cid);

	err = _ms_update_mmc_info(cid);

	/*Active flush */
	if (malloc_trim(0))
		MS_DBG("SUCCESS malloc_trim");

	return err;
}

void
ms_mmc_removed_handler(void)
{
	ms_scan_data_t *mmc_scan_data;

	mmc_scan_data = malloc(sizeof(ms_scan_data_t));

	mmc_scan_data->scan_type = MS_SCAN_VALID;
	mmc_scan_data->db_type = MS_MMC;

	g_async_queue_push(scan_queue, GINT_TO_POINTER(mmc_scan_data));

	/*remove added watch descriptors */
	ms_inoti_remove_watch_recursive(MS_MMC_ROOT_PATH);

	ms_inoti_delete_mmc_ignore_file();

	if (drm_svc_extract_ext_memory() == DRM_RESULT_SUCCESS)
		MS_DBG("drm_svc_extract_ext_memory OK");
}

void
ms_mmc_vconf_cb(void *data)
{
	MS_DBG_START();

	int status = 0;
	ms_scan_data_t *scan_data;

	MS_DBG("Received MMC noti from vconf : %d", status);

	if (!ms_config_get_int(VCONFKEY_SYSMAN_MMC_STATUS, &status)) {
		MS_DBG("........Get VCONFKEY_SYSMAN_MMC_STATUS failed........");
	}

	MS_DBG("ms_config_get_int : VCONFKEY_SYSMAN_MMC_STATUS END = %d",
	       status);

	mmc_state = status;

	if (current_usb_mode != VCONFKEY_USB_STORAGE_STATUS_OFF)
		return;

	if (mmc_state == VCONFKEY_SYSMAN_MMC_REMOVED ||
		mmc_state == VCONFKEY_SYSMAN_MMC_INSERTED_NOT_MOUNTED) {
		ms_mmc_removed_handler();
	}
	else if (mmc_state == VCONFKEY_SYSMAN_MMC_MOUNTED) {
		scan_data = malloc(sizeof(ms_scan_data_t));

		if (drm_svc_insert_ext_memory() == DRM_RESULT_SUCCESS)
			MS_DBG("drm_svc_insert_ext_memory OK");

		ms_make_default_path_mmc();

		scan_data->scan_type = ms_get_mmc_state();
		scan_data->db_type = MS_MMC;

		MS_DBG("ms_get_mmc_state is %d", scan_data->scan_type);

		g_async_queue_push(scan_queue, GINT_TO_POINTER(scan_data));
	}

	MS_DBG_END();

	return;
}

