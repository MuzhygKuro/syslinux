/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2009 Erwan Velu - All Rights Reserved
 *
 *   Permission is hereby granted, free of charge, to any person
 *   obtaining a copy of this software and associated documentation
 *   files (the "Software"), to deal in the Software without
 *   restriction, including without limitation the rights to use,
 *   copy, modify, merge, publish, distribute, sublicense, and/or
 *   sell copies of the Software, and to permit persons to whom
 *   the Software is furnished to do so, subject to the following
 *   conditions:
 *
 *   The above copyright notice and this permission notice shall
 *   be included in all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *   OTHER DEALINGS IN THE SOFTWARE.
 *
 * -----------------------------------------------------------------------
*/

#include "hdt-common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "syslinux/config.h"

void detect_parameters(int argc, char *argv[], struct s_hardware *hardware) {
 for (int i = 1; i < argc; i++) {
   if (!strncmp(argv[i], "modules=", 8)) {
      strncpy(hardware->modules_pcimap_path,argv[i]+8, sizeof(hardware->modules_pcimap_path));
   } else if (!strncmp(argv[i], "pciids=",7 )) {
      strncpy(hardware->pciids_path,argv[i]+7, sizeof(hardware->pciids_path));
   }
 }
}

void detect_syslinux(struct s_hardware *hardware) {
  hardware->sv = syslinux_version();
  switch(hardware->sv->filesystem) {
        case SYSLINUX_FS_SYSLINUX: strlcpy(hardware->syslinux_fs,"SYSlinux",9); break;
        case SYSLINUX_FS_PXELINUX: strlcpy(hardware->syslinux_fs,"PXElinux",9); break;
        case SYSLINUX_FS_ISOLINUX: strlcpy(hardware->syslinux_fs,"ISOlinux",9); break;
        case SYSLINUX_FS_EXTLINUX: strlcpy(hardware->syslinux_fs,"EXTlinux",9); break;
        case SYSLINUX_FS_UNKNOWN:
        default: strlcpy(hardware->syslinux_fs,"Unknown Bootloader",sizeof hardware->syslinux_fs); break;
  }
}

void init_hardware(struct s_hardware *hardware) {
  hardware->pci_ids_return_code=0;
  hardware->modules_pcimap_return_code=0;
  hardware->cpu_detection=false;
  hardware->pci_detection=false;
  hardware->disk_detection=false;
  hardware->dmi_detection=false;
  hardware->pxe_detection=false;
  hardware->nb_pci_devices=0;
  hardware->is_dmi_valid=false;
  hardware->is_pxe_valid=false;
  hardware->pci_domain=NULL;

  /* Cleaning structures */
  memset(hardware->disk_info,0,sizeof(hardware->disk_info));
  memset(&hardware->dmi,0,sizeof(s_dmi));
  memset(&hardware->cpu,0,sizeof(s_cpu));
  memset(&hardware->pxe,0,sizeof(struct s_pxe));
  memset(hardware->syslinux_fs,0,sizeof hardware->syslinux_fs);
  memset(hardware->pciids_path,0,sizeof hardware->pciids_path);
  memset(hardware->modules_pcimap_path,0,sizeof hardware->modules_pcimap_path);
  strcat(hardware->pciids_path,"pci.ids");
  strcat(hardware->modules_pcimap_path,"modules.pcimap");
}

/* Detecting if a DMI table exist
 * if yes, let's parse it */
int detect_dmi(struct s_hardware *hardware) {
  if (hardware->dmi_detection == true) return -1;
  hardware->dmi_detection=true;
  if (dmi_iterate(&hardware->dmi) == -ENODMITABLE ) {
	     hardware->is_dmi_valid=false;
             return -ENODMITABLE;
  }

  parse_dmitable(&hardware->dmi);
  hardware->is_dmi_valid=true;
 return 0;
}

/* Try to detects disk from port 0x80 to 0xff*/
void detect_disks(struct s_hardware *hardware) {
 hardware->disk_detection=true;
 for (int drive = 0x80; drive < 0xff; drive++) {
    if (get_disk_params(drive,hardware->disk_info) != 0)
          continue;
    struct diskinfo *d=&hardware->disk_info[drive];
    printf("  DISK 0x%X: %s : %s %s: sectors=%d, s/t=%d head=%d : EDD=%s\n",drive,d->aid.model,d->host_bus_type,d->interface_type, d->sectors, d->sectors_per_track,d->heads,d->edd_version);
 }
}

int detect_pxe(struct s_hardware *hardware) {
 void *dhcpdata;

 size_t dhcplen;
 t_PXENV_UNDI_GET_NIC_TYPE gnt;

 if (hardware->pxe_detection == true) return -1;
 hardware->pxe_detection=true;
 hardware->is_pxe_valid=false;
 memset(&gnt,0, sizeof(t_PXENV_UNDI_GET_NIC_TYPE));
 memset(&hardware->pxe,0, sizeof(struct s_pxe));

 /* This code can only work if pxelinux is loaded*/
 if (hardware->sv->filesystem != SYSLINUX_FS_PXELINUX) {
	 return -1;
 }

// printf("PXE: PXElinux detected\n");
 if (!pxe_get_cached_info(PXENV_PACKET_TYPE_DHCP_ACK, &dhcpdata, &dhcplen)) {
	pxe_bootp_t *dhcp=&hardware->pxe.dhcpdata;
	memcpy(&hardware->pxe.dhcpdata,dhcpdata,sizeof(hardware->pxe.dhcpdata));
	snprintf(hardware->pxe.mac_addr, sizeof(hardware->pxe.mac_addr), "%02x:%02x:%02x:%02x:%02x:%02x",
			dhcp->CAddr[0],dhcp->CAddr[1],dhcp->CAddr[2],dhcp->CAddr[3],dhcp->CAddr[4],dhcp->CAddr[5]);

	/* Saving Our IP address in a easy format*/
	hardware->pxe.ip_addr[0]= hardware->pxe.dhcpdata.yip & 0xff;
	hardware->pxe.ip_addr[1]= hardware->pxe.dhcpdata.yip >>8 & 0xff;
	hardware->pxe.ip_addr[2]= hardware->pxe.dhcpdata.yip >>16 & 0xff;
	hardware->pxe.ip_addr[3]= hardware->pxe.dhcpdata.yip >>24 & 0xff;

        if (!pxe_get_nic_type(&gnt)) {
	 switch(gnt.NicType) {
	         case PCI_NIC:
			 hardware->is_pxe_valid=true;
			 hardware->pxe.vendor_id=gnt.info.pci.Vendor_ID;
			 hardware->pxe.product_id=gnt.info.pci.Dev_ID;
			 hardware->pxe.subvendor_id=gnt.info.pci.SubVendor_ID;
			 hardware->pxe.subproduct_id=gnt.info.pci.SubDevice_ID,
			 hardware->pxe.rev=gnt.info.pci.Rev;
			 hardware->pxe.pci_bus= (gnt.info.pci.BusDevFunc >> 8) & 0xff;
			 hardware->pxe.pci_dev= (gnt.info.pci.BusDevFunc >> 3) & 0x7;
			 hardware->pxe.pci_func=gnt.info.pci.BusDevFunc & 0x03;
			 hardware->pxe.base_class=gnt.info.pci.Base_Class;
			 hardware->pxe.sub_class=gnt.info.pci.Sub_Class;
			 hardware->pxe.prog_intf=gnt.info.pci.Prog_Intf;
			 hardware->pxe.nictype=gnt.NicType;
			 break;
	         case CardBus_NIC:
			 hardware->is_pxe_valid=true;
			 hardware->pxe.vendor_id=gnt.info.cardbus.Vendor_ID;
			 hardware->pxe.product_id=gnt.info.cardbus.Dev_ID;
			 hardware->pxe.subvendor_id=gnt.info.cardbus.SubVendor_ID;
			 hardware->pxe.subproduct_id=gnt.info.cardbus.SubDevice_ID,
			 hardware->pxe.rev=gnt.info.cardbus.Rev;
			 hardware->pxe.pci_bus= (gnt.info.cardbus.BusDevFunc >> 8) & 0xff;
			 hardware->pxe.pci_dev= (gnt.info.cardbus.BusDevFunc >> 3) & 0x7;
			 hardware->pxe.pci_func=gnt.info.cardbus.BusDevFunc & 0x03;
			 hardware->pxe.base_class=gnt.info.cardbus.Base_Class;
			 hardware->pxe.sub_class=gnt.info.cardbus.Sub_Class;
			 hardware->pxe.prog_intf=gnt.info.cardbus.Prog_Intf;
			 hardware->pxe.nictype=gnt.NicType;
			 break;
		case PnP_NIC:
		default:  return -1; break;
        }
	/* Let's try to find the associated pci device */
	detect_pci(hardware);
	hardware->pxe.pci_device=NULL;
	hardware->pxe.pci_device_pos=0;
	struct pci_device *pci_device;
	int pci_number=0;
	for_each_pci_func(pci_device, hardware->pci_domain) {
		pci_number++;
		if ((__pci_bus == hardware->pxe.pci_bus) &&
		   (__pci_slot == hardware->pxe.pci_dev) &&
		   (__pci_func == hardware->pxe.pci_func) &&
		   (pci_device->vendor == hardware->pxe.vendor_id) &&
		   (pci_device->product == hardware->pxe.product_id)) {
			   hardware->pxe.pci_device=pci_device;
			   hardware->pxe.pci_device_pos=pci_number;
		   }
	}
       }
 }
 return 0;
}

void detect_pci(struct s_hardware *hardware) {
  if (hardware->pci_detection == true) return;
  hardware->pci_detection=true;

  /* Scanning to detect pci buses and devices */
  hardware->pci_domain = pci_scan();

  hardware->nb_pci_devices=0;
  struct pci_device *pci_device;
  for_each_pci_func(pci_device, hardware->pci_domain) {
          hardware->nb_pci_devices++;
  }

  printf("PCI: %d devices detected\n",hardware->nb_pci_devices);
  printf("PCI: Resolving names\n");
  /* Assigning product & vendor name for each device*/
  hardware->pci_ids_return_code=get_name_from_pci_ids(hardware->pci_domain, hardware->pciids_path);

  printf("PCI: Resolving class names\n");
  /* Assigning class name for each device*/
  hardware->pci_ids_return_code=get_class_name_from_pci_ids(hardware->pci_domain, hardware->pciids_path);


  printf("PCI: Resolving module names\n");
  /* Detecting which kernel module should match each device */
  hardware->modules_pcimap_return_code=get_module_name_from_pci_ids(hardware->pci_domain,hardware->modules_pcimap_path);

  /* we try to detect the pxe stuff to populate the PXE: field of pci devices */
  detect_pxe(hardware);
}

void cpu_detect(struct s_hardware *hardware) {
  if (hardware->cpu_detection == true) return;
  detect_cpu(&hardware->cpu);
  hardware->cpu_detection=true;
}

/* Find the last instance of a particular command line argument
   (which should include the final =; do not use for boolean arguments) */
char *find_argument(const char **argv, const char *argument)
{
  int la = strlen(argument);
  const char **arg;
  char *ptr = NULL;

  for (arg = argv; *arg; arg++) {
    if (!memcmp(*arg, argument, la))
      ptr = *arg + la;
  }

  return ptr;
}

void clear_screen(void)
{
  fputs("\033e\033%@\033)0\033(B\1#0\033[?25l\033[2J", stdout);
  display_line_nb=0;
}
