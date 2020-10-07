
#include <stdio.h>

#include <LCUI.h>
#include <LCUI/thread.h>
#include <LCUI/graph.h>
#include <LCUI/image.h>

#include "build.h"
#include "file_storage.h"

typedef struct FileStorageTestContextRec {
	int storage;
	int requests;
	LCUI_Cond cond;
	LCUI_Mutex mutex;
} FileStorageTestContextRec, *FileStorageTestContext;

static void OnGetThumbnail(FileStatus *status, LCUI_Graph *thumb, void *data)
{
	FileStorageTestContext ctx = data;
	ctx->requests--;
	printf("[%d] thumbnail received\n", ctx->requests);
	if (ctx->requests == 0) {
		LCUIMutex_Lock(&ctx->mutex);
		LCUICond_Signal(&ctx->cond);
		LCUIMutex_Unlock(&ctx->mutex);
		printf("all thumbnails received\n");
	}
}

static void CreateExmapleImage(const char *filename)
{
	LCUI_Graph image;

	Graph_Init(&image);
	image.color_type = LCUI_COLOR_TYPE_ARGB;
	Graph_Create(&image, 1920, 1080);
	Graph_FillRect(&image, ARGB(255, 255, 0, 0), NULL, TRUE);
	LCUI_WritePNGFile(filename, &image);
	Graph_Free(&image);
}

static void test_file_storage_image(void)
{
	int i;
	FileStorageTestContext ctx;

	ctx = malloc(sizeof(FileStorageTestContextRec));
	LCUICond_Init(&ctx->cond);
	LCUIMutex_Init(&ctx->mutex);
	ctx->storage = FileStorage_Connect();
	CreateExmapleImage("test_file_storage_image.png");
	for (i = 0, ctx->requests = 0; i < 5; ++i, ++ctx->requests) {
		FileStorage_GetThumbnail(ctx->storage,
					 L"test_file_storage_image.png", 200,
					 200, OnGetThumbnail, ctx);
	}
	LCUIMutex_Lock(&ctx->mutex);
	while (ctx->requests > 0) {
		LCUICond_Wait(&ctx->cond, &ctx->mutex);
	}
	LCUIMutex_Unlock(&ctx->mutex);
	FileStorage_Close(ctx->storage);
	remove("test_file_storage_image.png");
}

static void test_file_storage(void)
{
	FileStorage_Init();
	test_file_storage_image();
	FileStorage_Free();
}

int main(void)
{
	test_file_storage();
	return 0;
}
