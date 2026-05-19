# CubeScene

Exercício `CubeScene` em OpenGL com:
- cubo colorido por face (malha triangular),
- rotação nos eixos X/Y/Z,
- translação e escala uniforme por teclado,
- múltiplas instâncias de cubo,
- leitura de malha 3D `.obj` (somente geometria).

## Leitor de malha 3D (OBJ)

- Caminho inicial configurado no código: `../assets/Modelos3D/Cube.obj`.
- Nesta etapa, o leitor usa somente a geometria (posições de vértices):
  - lê linhas `v` e `f`,
  - aceita faces `f v`, `f v/vt/vn` e `f v//vn`,
  - triangula faces com mais de 3 vértices usando fan triangulation.
- Materiais (`.mtl`), texturas e normais não são usados na renderização nesta atividade.
- Se houver erro de leitura/parsing do `.obj`, o `CubeScene` usa automaticamente um cubo hardcoded de fallback.

## Pré-requisitos

- GLAD configurada no projeto (`include/glad` e `common/glad.c`)
- CMake instalado
- GLFW disponível no sistema
  - macOS (Homebrew): `brew install glfw`

## Comandos necessários

Na raiz do projeto:

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

## Executar o CubeScene

No diretório `build`:

```bash
./CubeScene
```

No Windows (PowerShell/CMD):

```bat
CubeScene.exe
```

## Controles do teclado

- `X`, `Y`, `Z`: seleciona rotação do cubo ativo no respectivo eixo
- `W`, `S`: move no eixo Z (frente/trás)
- `A`, `D`: move no eixo X (esquerda/direita)
- `I`, `J`: move no eixo Y (cima/baixo)
- `[` e `]`: escala uniforme (diminuir/aumentar)
- `N`: cria novo cubo
- `TAB`: alterna cubo ativo
- `ESC`: fecha a janela
