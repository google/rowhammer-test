
# Vendor responses to the rowhammer bug

This is a list of public vendor responses to the rowhammer bug,
approximately in the order they were initially published.

* Cisco, 2015/03/09:
  * "Mitigations Available for the DRAM Row Hammer Vulnerability",
    http://blogs.cisco.com/security/mitigations-available-for-the-dram-row-hammer-vulnerability
  * Cisco Security Advisory,
    http://tools.cisco.com/security/center/content/CiscoSecurityAdvisory/cisco-sa-20150309-rowhammer:
    "No Cisco products are known to be affected by the Row Hammer
    Privilege Escalation Attack."

* IBM, 2015/03/09: IBM Product Security Incident Response Blog,
  https://www-304.ibm.com/connections/blogs/PSIRT/entry/exploiting_the_dram_row_hammer_bug?lang=en_us
  * "IBM has determined that all IBM System z, System p, System x, and
    IBM Storage products are not vulnerable to this attack. IBM is
    analyzing other IBM products to determine if they are potentially
    impacted by this issue."

* Linux kernel, 2015/03/09: Change "pagemap: do not leak physical
  addresses to non-privileged userspace",
  http://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/commit/?id=ab676b7d6fbf4b294bf198fb27ade5b0e865c7ce
  (committed 2015/03/17)

* HP, 2015/03/13 to 2015/04/07: "Notice: HP Servers - "Rowhammer"
  Security Vulnerability",
  http://h20564.www2.hp.com/hpsc/doc/public/display?docId=emr_na-c04593978&sp4ts.oid=7398911

* Lenovo, 2015/03/13 to 2015/06/30: "Row Hammer Privilege Escalation",
  LEN-2015-009,
  http://support.lenovo.com/us/en/product_security/row_hammer
  * Lists various BIOS updates.

* RedHat, 2015/03/13: "DRAM-Related Faults (rowhammer)",
  https://access.redhat.com/articles/1377393

* Microsoft, 2015/03/16: "Microsoft Azure uses Error-Correcting Code
  memory for enhanced reliability and security",
  http://azure.microsoft.com/blog/2015/03/16/microsoft-azure-uses-error-correcting-code-memory-for-enhanced-reliability-and-security/

* Apple, 2015/06/30:
  * Mac EFI Security Update 2015-001, https://support.apple.com/en-us/HT204934
  * CVE-2015-3693
  * "Available for: OS X Mountain Lion v10.8.5, OS X Mavericks v10.9.5"
  * "Description: A disturbance error, also known as Rowhammer, exists
    with some DDR3 RAM that could have led to memory corruption. This
    issue was mitigated by increasing memory refresh rates."
