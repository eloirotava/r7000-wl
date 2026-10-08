/*
 * osl_compat.h - APIs do kernel 4.4 usadas pelo shared/ da Broadcom e
 * removidas ate o 6.12.  Incluido em todos os fontes (-include).
 */
#ifndef _OSL_COMPAT_H_
#define _OSL_COMPAT_H_

#include <linux/version.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>

/* API DMA pci_* (removida no 5.18) */
#ifndef PCI_DMA_TODEVICE
#define PCI_DMA_BIDIRECTIONAL	DMA_BIDIRECTIONAL
#define PCI_DMA_TODEVICE	DMA_TO_DEVICE
#define PCI_DMA_FROMDEVICE	DMA_FROM_DEVICE
#define PCI_DMA_NONE		DMA_NONE

static inline void *pci_alloc_consistent(struct pci_dev *pdev, size_t size,
	dma_addr_t *handle)
{
	return dma_alloc_coherent(&pdev->dev, size, handle, GFP_ATOMIC);
}

static inline void pci_free_consistent(struct pci_dev *pdev, size_t size,
	void *va, dma_addr_t handle)
{
	dma_free_coherent(&pdev->dev, size, va, handle);
}

static inline dma_addr_t pci_map_single(struct pci_dev *pdev, void *ptr,
	size_t size, int dir)
{
	return dma_map_single(&pdev->dev, ptr, size, (enum dma_data_direction)dir);
}

static inline void pci_unmap_single(struct pci_dev *pdev, dma_addr_t addr,
	size_t size, int dir)
{
	dma_unmap_single(&pdev->dev, addr, size, (enum dma_data_direction)dir);
}

static inline int pci_map_sg(struct pci_dev *pdev, struct scatterlist *sg,
	int nents, int dir)
{
	return dma_map_sg(&pdev->dev, sg, nents, (enum dma_data_direction)dir);
}

static inline void pci_unmap_sg(struct pci_dev *pdev, struct scatterlist *sg,
	int nents, int dir)
{
	dma_unmap_sg(&pdev->dev, sg, nents, (enum dma_data_direction)dir);
}
#endif

#ifndef ioremap_nocache
#define ioremap_nocache(pa, sz)	ioremap((pa), (sz))
#endif

/*
 * Janela ACP (DMA coerente) do kernel Broadcom: no mainline o DMA do
 * BCM5301X nao e coerente e a API dma_* cuida do cache, entao janela 0.
 */
extern unsigned int ns_acp_win_size;

#endif /* _OSL_COMPAT_H_ */
