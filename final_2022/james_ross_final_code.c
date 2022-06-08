
#define ONE_K 1024
#define CMD_TRANS_CSUM_ON 0x00F0

struct my_desc {
    __le64 buffer_addr;
    union {
        __le64 data;
        struct {
            u8 status;
            __le16 error;
            __le16 cmd;
            __le16 len;
            __le8 reserved;
        } feilds;
    } desc_data;
};

struct buffer_info {
    dma_addr_t dma;
    size_t size;
};

struct rx_ring {
    /* assuming 1 descriptor for sake of example */
    void *desc;
    struct buffer_info buf_info;
    void *buffer; 
};

int alloc_buffer(struct rx_ring *rx_ring)
{
    rx_ring->buff_info.size = ONE_K;
    rx_ring->buffer = kzalloc(rx_ring->buff_info.size);
    if (unlikely(!rx_ring->buffer))
        return -ENOMEM;
    return 0;
}

int ece_pin_buffer_fill_descriptor(struct pci_dev *pdev, 
                                   struct rx_ring *rx_ring)
{
    struct my_desc *desc = rx_ring->desc;
    struct buffer_info *buff_info = &rx_ring->buff_info;

    /* pin buffer memory */
    rx_ring->buff_info.dma = dma_map_single(&pdev->dev, 
                                            rx_ring->buffer,
                                            buff_info->size, 
                                            DMA_TO_DEVICE);
    if (unlikely(!buff_info.dma)) {
        /* 
         * not sure whats a better error for failed pin suggestions would be 
         * appreciated! 
         */
        return -ENOMEM; 
    }

    /* set dma addr */
    desc->buffer_addr = cpu_to_le64(buff_info->dma);

    /* set length */
    desc->desc_data.feilds.len = cpu_to_le16(buff_info->size);

    /* clear status */
    desc->desc_data.feilds.status = 0;

    /* start transmit with computing checksum */
    desc->desc_data.feilds.cmd = cpu_to_le16(CMD_TRANS_CSUM_ON);

    return 0;
}

int exec_ece_final(struct pci_dev *pdev, struct rx_ring *rx_ring)
{
    int err;
    err = alloc_buffer(rx_ring);
    if (unlikely(err))
        return err;
    
    err = ece_pin_buffer_fill_descriptor(pdev, rx_ring);
    if (unlikely(err))
        return err;
    return 0;
}
