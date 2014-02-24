help: help_customer

	@echo =======================================================================
	@echo The mark in board is "Mesonboard_AML8726-M_V1.1"
	@echo config command: \"make arm_8726m_config\"

	@echo =======================================================================
	@echo The mark in board is "M3_SKT_V1"
	@echo config command: \"make m3_skt_v1_config\"

	@echo =======================================================================
	@echo The mark in board is "WA_M3_REF_V1"
	@echo config command: \"make m3_wa_ref_v1_config\"

	@echo =======================================================================
	@echo The mark in board is "M6_SKT_V1"
	@echo config command: \"make m6_skt_v1_config\"

	@echo =======================================================================
	@echo The mark in board is "M6_REF_V1"
	@echo config command: \"make m6_ref_v1_config\"

	@echo =======================================================================
	@echo The mark in board is "M6_REF_V2"
	@echo config command: \"make m6_ref_v2_config\"

	@echo =======================================================================
	@echo The mark in board is "M6S_SKT_V1", relative to chip M6S
	@echo config command: \"make m6s_skt_v1_config\"

	@echo =======================================================================
	@echo The mark in board is "M6L_SKT_V1", relative to chip M6l
	@echo config command: \"make m6l_skt_v1_config\"

	@echo =======================================================================
	@echo The mark in board is "M6TV_SKT_V1"
	@echo config command: \"make m6tv_skt_v1_config\"

	@echo =======================================================================
	@echo The mark in board is "M6TV_REF_V1"
	@echo config command: \"make m6tv_ref_v1_config\"
	
	@echo

sinclude $(wildcard $(SRCTREE)/customer/board/Readme.mk)
