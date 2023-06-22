%bcond_without tests
%bcond_without weak_deps

%global __os_install_post %(echo '%{__os_install_post}' | sed -e 's!/usr/lib[^[:space:]]*/brp-python-bytecompile[[:space:]].*$!!g')
%global __provides_exclude_from ^/opt/ros/rolling/.*$
%global __requires_exclude_from ^/opt/ros/rolling/.*$

Name:           ros-rolling-canopen-utils
Version:        0.2.3
Release:        1%{?dist}%{?release_suffix}
Summary:        ROS canopen_utils package

License:        Apache-2.0
Source0:        %{name}-%{version}.tar.gz

Requires:       ros-rolling-canopen-interfaces
Requires:       ros-rolling-lifecycle-msgs
Requires:       ros-rolling-rclpy
Requires:       ros-rolling-std-msgs
Requires:       ros-rolling-ros-workspace
BuildRequires:  python%{python3_pkgversion}-devel
BuildRequires:  ros-rolling-canopen-interfaces
BuildRequires:  ros-rolling-lifecycle-msgs
BuildRequires:  ros-rolling-rclpy
BuildRequires:  ros-rolling-ros-workspace
BuildRequires:  ros-rolling-std-msgs
Provides:       %{name}-devel = %{version}-%{release}
Provides:       %{name}-doc = %{version}-%{release}
Provides:       %{name}-runtime = %{version}-%{release}

%if 0%{?with_tests}
BuildRequires:  ros-rolling-ament-lint-auto
%endif

%description
Utils for working with ros2_canopen.

%prep
%autosetup -p1

%build
# In case we're installing to a non-standard location, look for a setup.sh
# in the install tree and source it.  It will set things like
# CMAKE_PREFIX_PATH, PKG_CONFIG_PATH, and PYTHONPATH.
if [ -f "/opt/ros/rolling/setup.sh" ]; then . "/opt/ros/rolling/setup.sh"; fi
%py3_build

%install
# In case we're installing to a non-standard location, look for a setup.sh
# in the install tree and source it.  It will set things like
# CMAKE_PREFIX_PATH, PKG_CONFIG_PATH, and PYTHONPATH.
if [ -f "/opt/ros/rolling/setup.sh" ]; then . "/opt/ros/rolling/setup.sh"; fi
%py3_install -- --prefix "/opt/ros/rolling"

%if 0%{?with_tests}
%check
# Look for a directory with a name indicating that it contains tests
TEST_TARGET=$(ls -d * | grep -m1 "\(test\|tests\)" ||:)
if [ -n "$TEST_TARGET" ] && %__python3 -m pytest --version; then
# In case we're installing to a non-standard location, look for a setup.sh
# in the install tree and source it.  It will set things like
# CMAKE_PREFIX_PATH, PKG_CONFIG_PATH, and PYTHONPATH.
if [ -f "/opt/ros/rolling/setup.sh" ]; then . "/opt/ros/rolling/setup.sh"; fi
%__python3 -m pytest $TEST_TARGET || echo "RPM TESTS FAILED"
else echo "RPM TESTS SKIPPED"; fi
%endif

%files
/opt/ros/rolling

%changelog
* Thu Jun 22 2023 christoph <christoph.hellmann.santos@ipa.fraunhofer.de> - 0.2.3-1
- Autogenerated by Bloom

* Wed Jun 21 2023 christoph <christoph.hellmann.santos@ipa.fraunhofer.de> - 0.2.2-1
- Autogenerated by Bloom

* Wed Jun 21 2023 christoph <christoph.hellmann.santos@ipa.fraunhofer.de> - 0.2.1-1
- Autogenerated by Bloom

* Wed Jun 14 2023 christoph <christoph.hellmann.santos@ipa.fraunhofer.de> - 0.2.0-1
- Autogenerated by Bloom

