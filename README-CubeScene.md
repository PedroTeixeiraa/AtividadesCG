# CubeScene

Exercício `CubeScene` em OpenGL com:
- cubo colorido por face (malha triangular),
- rotação nos eixos X/Y/Z,
- translação e escala uniforme por teclado,
- múltiplas instâncias de cubo,
- leitura de malha 3D `.obj` com coordenadas de textura,
- leitura básica de material `.mtl` (apenas `map_Kd`) para textura difusa.

## Leitor de malha 3D (OBJ + MTL)

- Caminho inicial configurado no código: `../assets/Modelos3D/Suzanne.obj`.
- O carregador:
  - lê linhas `v`, `vt`, `f`, `mtllib` e `usemtl`,
  - aceita faces `f v`, `f v/vt/vn` e `f v//vn`,
  - triangula faces com mais de 3 vértices usando fan triangulation,
  - armazena posição + UV como atributos de vértice.
- No `.mtl`, nesta etapa, é lido apenas:
  - `newmtl`
  - `map_Kd` (nome do arquivo de textura difusa).
- O shader usa textura quando `map_Kd` é encontrado e a imagem é carregada.
- Se houver falha de leitura/parsing de `.obj`, `.mtl` ou textura, o `CubeScene` faz fallback para o cubo hardcoded colorido.

## Limitações da etapa

- Apenas o `map_Kd` é considerado do `.mtl`.
- Em arquivos com múltiplos materiais/texturas, é usada a primeira textura difusa válida encontrada para o material ativo.
- Normais (`vn`) do OBJ não são usadas nesta etapa.

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
