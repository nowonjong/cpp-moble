# 외부 라이브러리

## SFML 3.0.2

- 공식 다운로드: https://www.sfml-dev.org/download/sfml/3.0.2/
- 사용 파일: SFML-3.0.2-windows-vc17-64-bit.zip
- 실제 URL: https://www.sfml-dev.org/files/SFML-3.0.2-windows-vc17-64-bit.zip
- 대상: Visual C++ 2022(v143), Windows x64, C++17.
- 내려받은 ZIP SHA-256: `979756B373901CA8F58D8D187AF3BA6338C4F23078F2A6AB2E8468B26DED6FBA`.
- 공유용 복사본에는 용량을 줄이기 위해 원본 ZIP과 `doc`·`examples`, 미사용 Audio·Network 라이브러리를 제외했다. 빌드에 필요한 Graphics·Window·System의 `include`·`lib`·`bin`과 라이선스는 포함했다.
- SFML은 Laurent Gomila와 SFML 기여자들이 만든 라이브러리이며, 게임 팀이 구현한 코드와 구분한다.

SFML의 Graphics, Window, System 모듈만 사용한다. 버전 3.0.2를 고정해 기존 C++17 설정과 맞췄다. 다른 버전, MinGW용 또는 32비트용 SDK로 폴더를 교체하지 않는다.

## 프로젝트 연결과 실행 파일

루트 `Sfml.props`를 프로젝트의 기존 `ItemDefinitionGroup` 뒤이면서 `Microsoft.Cpp.targets` 가져오기 바로 앞에서 가져온다.

```xml
<Import Project="Sfml.props" />
<Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />
```

| 구성 | 런타임 | 링크 라이브러리 | 실행 파일 옆 SFML DLL |
| --- | --- | --- | --- |
| Debug | /MDd | sfml-graphics-d / sfml-window-d / sfml-system-d | sfml-graphics-d-3.dll / sfml-window-d-3.dll / sfml-system-d-3.dll |
| Release | /MD | sfml-graphics / sfml-window / sfml-system | sfml-graphics-3.dll / sfml-window-3.dll / sfml-system-3.dll |

빌드하면 해당 구성의 SFML DLL과 라이선스가 출력 폴더에 복사된다. Release는 설치된 Visual Studio의 재배포 가능한 x64 CRT DLL도 함께 복사한다. 현재 SDK DLL이 사용하는 핵심 CRT는 `msvcp140.dll`, `vcruntime140.dll`, `vcruntime140_1.dll`이며, 빌드된 프로그램의 요구를 함께 충족하도록 Microsoft.VC143.CRT 폴더의 DLL을 복사한다.

- 다른 PC에서 실행할 때는 **Release 출력 폴더 전체**와 사전/기록 폴더를 함께 전달한다.
- Debug는 개발 PC의 디버그 런타임을 사용한다. 디버그 CRT는 재배포하지 않는다.
- Windows 10/11 x64와 SFML이 사용할 수 있는 그래픽 드라이버가 전제다.
- CRT 경로를 찾지 못한 다른 개발 PC에서는 빌드 경고가 표시되며, 해당 PC의 Visual C++ 재배포 패키지 설치 여부를 확인한다.
- 전역 라이브러리 경로 설정이나 자동 설치 프로그램 실행은 하지 않았다.

공식 설정 설명: https://www.sfml-dev.org/tutorials/3.0/getting-started/visual-studio/

Microsoft CRT 출처/배포 설명: https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files?view=msvc-170

