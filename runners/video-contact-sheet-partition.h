#ifndef VIDEO_CONTACT_SHEET_PARTITION_H
#define VIDEO_CONTACT_SHEET_PARTITION_H

/* Independent ST instances divide global indexes, never the sampling interval. */
static int contact_sheet_partition_valid(int index, int count)
{
    return count >= 1 && count <= 2 && index >= 0 && index < count;
}

static int contact_sheet_partition_owns(int sample_index, int index, int count)
{
    return contact_sheet_partition_valid(index, count) &&
           sample_index >= 0 && sample_index % count == index;
}

static int contact_sheet_partition_size(int sample_count, int index, int count)
{
    if (!contact_sheet_partition_valid(index, count) || sample_count <= index)
        return 0;
    return (sample_count - 1 - index) / count + 1;
}

#endif
