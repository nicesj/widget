Name: libwidget_edje
Summary: EDJE Script loader for the data provider master
Version: 1.0.0
Release: 1
Group: Applications/Core Applications
License: Flora-1.1
Source0: %{name}-%{version}.tar.gz
Source1001: %{name}.manifest
BuildRequires: cmake, gettext-tools, coreutils
BuildRequires: pkgconfig(dlog)
BuildRequires: pkgconfig(eina)
BuildRequires: pkgconfig(evas)
BuildRequires: pkgconfig(edje)
BuildRequires: pkgconfig(eet)
BuildRequires: pkgconfig(vconf)
BuildRequires: pkgconfig(ecore)
BuildRequires: pkgconfig(elementary)
BuildRequires: pkgconfig(widget_service)
BuildRequires: pkgconfig(capi-system-system-settings)
BuildRequires: model-build-features

%description
Plugin for the data provider master to load the edje scripts

%prep
%setup -q
cp %{SOURCE1001} .

%build
%if 0%{?sec_build_binary_debug_enable}
export CFLAGS="$CFLAGS -DTIZEN_DEBUG_ENABLE"
export CXXFLAGS="$CXXFLAGS -DTIZEN_DEBUG_ENABLE"
export FFLAGS="$FFLAGS -DTIZEN_DEBUG_ENABLE"
%endif

%if 0%{?tizen_build_binary_release_type_eng}
export CFLAGS="${CFLAGS} -DTIZEN_ENGINEER_MODE"
export CXXFLAGS="${CXXFLAGS} -DTIZEN_ENGINEER_MODE"
export FFLAGS="${FFLAGS} -DTIZEN_ENGINEER_MODE"
%endif
%cmake .
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install
mkdir -p %{buildroot}/%{_datarootdir}/license

%post

%files -n libwidget_edje
%manifest %{name}.manifest
%defattr(-,root,root,-)
/usr/share/data-provider-master/plugin-script/*.so*
%{_datarootdir}/license/*

# End of a file
