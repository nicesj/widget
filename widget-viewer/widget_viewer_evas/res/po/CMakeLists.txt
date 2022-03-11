# for i18n

SET(POFILES
	ar.po
	az.po
	bg.po
	bn.po
	ca.po
	cs.po
	da.po
	de.po
	el_GR.po
	en.po
	en_PH.po
	en_US.po
	es_ES.po
	es_US.po
	et.po
	eu.po
	fa.po
	fi.po
	fr.po
	fr_CA.po
	ga.po
	gl.po
	gu.po
	he.po
	hi.po
	hr.po
	hu.po
	hy.po
	is.po
	it_IT.po
	ja_JP.po
	ka.po
	kk.po
	kn.po
	ko_KR.po
	lt.po
	lv.po
	mk.po
	ml.po
	nb.po
	nl.po
	pl.po
	pt_BR.po
	pt_PT.po
	ro.po
	ru_RU.po
	si.po
	sk.po
	sl.po
	sr.po
	sv.po
	ta.po
	te.po
	th.po
	tr_TR.po
	uk.po
	ur.po
	uz.po
	zh_CN.po
	zh_HK.po
	zh_TW.po
)

SET(MSGFMT "/usr/bin/msgfmt")
FOREACH(pofile ${POFILES})
	SET(pofile ${CMAKE_CURRENT_SOURCE_DIR}/${pofile})
	MESSAGE(STATUS "  ${pofile}")
	GET_FILENAME_COMPONENT(absPofile ${pofile} ABSOLUTE)
	GET_FILENAME_COMPONENT(lang ${absPofile} NAME_WE)
	SET(moFile ${CMAKE_CURRENT_BINARY_DIR}/${lang}.mo)
	ADD_CUSTOM_COMMAND(
			OUTPUT ${moFile}
			COMMAND ${MSGFMT} -o ${moFile} ${absPofile}
			DEPENDS ${absPofile}
	)
	INSTALL(FILES ${moFile}
			DESTINATION ${LOCALEDIR}/${lang}/LC_MESSAGES RENAME ${PROJECT_NAME}.mo)
	SET(moFiles ${moFiles} ${moFile})
ENDFOREACH(pofile)

ADD_CUSTOM_TARGET(po ALL DEPENDS ${moFiles})
