# Como rodar o Ateliê do Filé

Instruções para instalar as dependências, compilar, executar e testar o projeto no Windows.

## 1. Dependências

| Finalidade | Necessário |
| --- | --- |
| Executar o jogo compilado | Windows 10/11 de 64 bits, driver da placa de vídeo com suporte a OpenGL 3.3 Core e a pasta `assets` ao lado do executável. |
| Compilar o projeto | Visual Studio 2022 ou Build Tools 2022, com MSVC v143 para x64/x86 e um Windows 10/11 SDK. |
| Configurar a compilação | CMake 3.20 ou superior; pode ser a versão instalada com o Visual Studio. |
| Usar os scripts | PowerShell. |
| Versionar ou clonar | Git; não é necessário para executar o jogo. |

O projeto usa C++17. A versão atual é nativa para Windows e utiliza Win32, WGL, GDI e WIC. As bibliotecas `opengl32`, `user32`, `gdi32`, `ole32` e `windowscodecs` são fornecidas pelo Windows/SDK. O CMake configura o runtime C++ estático em builds MSVC.

Não há bibliotecas externas para baixar pelo CMake. Godot, Unity, Three.js, GLFW, GLAD, GLM, Node.js e Python não são dependências desta versão. O jogo funciona sem internet depois que os arquivos necessários estão no computador.

## 2. Preparar o Visual Studio 2022

Abra **Visual Studio Installer**, selecione a instalação **2022** e clique em **Modificar**. Marque **Desenvolvimento para desktop com C++ / Desktop development with C++** e confira estes componentes:

- MSVC v143 — ferramentas de compilação C++ x64/x86.
- Windows 10 SDK ou Windows 11 SDK.
- C++ CMake tools for Windows.

O script de compilação procura especificamente uma instalação do Visual Studio 2022. Apenas instalar o Visual Studio Code ou outra versão do Visual Studio não satisfaz essa procura.

Referências: [instalação do suporte C++](https://learn.microsoft.com/en-us/cpp/build/vscpp-step-0-installation?view=msvc-170), [alteração de componentes pelo Installer](https://learn.microsoft.com/en-us/visualstudio/install/modify-visual-studio?view=vs-2022) e [download do CMake](https://cmake.org/download/). O CMake separado é opcional quando o componente do Visual Studio está instalado.

## 3. Executar o pacote Windows

1. Extraia o ZIP completo.
2. Abra `AtelieFileOpenGL.exe` dentro da pasta extraída.
3. Mantenha `assets` no mesmo diretório do executável, incluindo `shaders` e `textures`.

Quando fornecido separadamente, o pacote Windows está em `dist\AtelieFileOpenGL-0.1-windows.zip`. Pacotes e executáveis não são versionados e não acompanham um clone do repositório. Para executar a partir das fontes clonadas, siga a seção seguinte. Não é necessário instalar o Visual Studio para usar um pacote já compilado.

## 4. Compilar e executar as fontes

Abra o PowerShell na raiz da pasta clonada ou extraída, onde estão `CMakeLists.txt`, `abrir.ps1` e a pasta `src`. Execute:

```powershell
.\scripts\build.ps1 -Test
.\abrir.ps1
```

O primeiro comando de script configura e compila em Release x64 e executa os testes de lógica. O script detecta o Visual Studio 2022 e procura o CMake no PATH ou na própria instalação do Visual Studio.

O executável fica em:

```text
build\bin\AtelieFileOpenGL.exe
```

Para abrir novamente, execute `abrir.ps1` ou abra o executável. Se ainda não houver executável, `abrir.ps1` chama a compilação antes de iniciar o jogo.

Para compilar em Debug:

```powershell
.\scripts\build.ps1 -Configuration Debug -Test
```

## 5. Compilar sem os scripts

No terminal de desenvolvimento do Visual Studio, com CMake disponível, use os comandos abaixo. Ajuste o caminho de `CMAKE_GENERATOR_INSTANCE` se a instalação 2022 for Build Tools ou estiver em outra pasta.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_GENERATOR_INSTANCE="C:\Program Files\Microsoft Visual Studio\2022\Community"
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
.\build\bin\AtelieFileOpenGL.exe
```

## 6. Controles

| Contexto | Comando | Ação |
| --- | --- | --- |
| Exploração | WASD | Caminhar. |
| Exploração | Mouse / roda | Olhar / ajustar a distância da câmera. |
| Exploração | E | Interagir com o objeto indicado: porta, luminária ou bancada. |
| Bastidor | 1, 2, 3 | Escolher coral, azul ou amarelo. |
| Bastidor | Clique esquerdo / direito | Tecer / apagar uma célula à direita. |
| Bastidor | V | Conferir a simetria. |
| Bastidor | X | Mostrar ou ocultar o exemplo. |
| Bastidor | R; Enter / Esc | Solicitar reinício; confirmar / cancelar. |
| Bastidor | Q | Voltar ao ateliê. |
| Geral | Esc | Pausar ou continuar. |
| Geral | F1 / F2 | Ajuda / demonstração de transformações. |
| Modo CG | Setas; Z/C; −/=; Backspace | Transladar; girar; escalar; restaurar o novelo. |
| Geral | F12 / Alt+F4 | Salvar captura / encerrar. |

## 7. Verificação gráfica

```powershell
.\build\bin\AtelieFileOpenGL.exe --smoke-test --output evidence
```

Esse comando abre um contexto OpenGL, verifica a integração do jogo, salva capturas e termina automaticamente. Os resultados ficam em `evidence\runtime.log`. A pasta de evidências não é versionada.

## 8. Problemas comuns

| Mensagem ou sintoma | O que conferir |
| --- | --- |
| Visual Studio 2022 não encontrado | Instalação 2022 e componente MSVC v143 x64/x86 no Installer. |
| CMake não encontrado | Componente C++ CMake tools for Windows ou instalação de CMake no PATH. |
| Windows SDK ausente | Instalação de um Windows 10/11 SDK pelo Installer. |
| OpenGL 3.3 Core indisponível | Driver do fabricante da GPU e suporte da placa ao perfil exigido. |
| Shader ou textura não encontrado | Pasta `assets` completa ao lado do executável. |
| PowerShell bloqueia os scripts | Use os comandos diretos do CMake em um terminal de desenvolvimento com as ferramentas configuradas. |
| Controles pausados após alternar de janela | Clique na janela e pressione Esc para continuar. |
| Jogo fecha ao iniciar | Execute o `.exe` pelo terminal para ler a mensagem de erro. |

## 9. Estrutura necessária

```text
CMakeLists.txt
COMO-RODAR.md
abrir.ps1
scripts/build.ps1
src/
tests/
assets/shaders/
assets/textures/
```

Mantenha essa estrutura ao baixar as fontes. A compilação copia `assets` para a pasta do executável. A origem e a licença das imagens estão em `assets/textures/sources.json`.
